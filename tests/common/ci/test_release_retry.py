"""Exercise release recovery against a stateful fake API; never contact GitHub."""

import contextlib
import io
import os
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[3]
for folder in ("build", "build/ci/common", "build/acceptance/common"):
    sys.path.insert(0, str(ROOT / folder))
from workspace import TemporaryDirectory

from release_pipeline import ApiError, TransientApiError, main, prepare_draft_with_retries
from release_support import ReleaseError
from test_release_pipeline import COMMIT, OTHER_COMMIT, FakeApi, packages, EXPECTED_PAIRS


class ReleaseRetryTests(unittest.TestCase):
    def setUp(self):
        self.items = packages()
        self.delays = []
        self.stdout, self.stderr = io.StringIO(), io.StringIO()
        self.enterContext(contextlib.redirect_stdout(self.stdout))
        self.enterContext(contextlib.redirect_stderr(self.stderr))

    def run_draft(self, api, *, sleep=None):
        return prepare_draft_with_retries(
            api, "v1.2.3", COMMIT, False, self.items, expected_pairs=EXPECTED_PAIRS,
            sleep=self.delays.append if sleep is None else sleep)

    def test_transient_read_recovers_and_reports_the_retry(self):
        for status in (500, 502, 503, 504):
            with self.subTest(status=status):
                api = FakeApi()
                request = api.request
                failed = False
                self.delays.clear()

                def fail_once(method, path, data=None):
                    nonlocal failed
                    if not failed:
                        failed = True
                        raise ApiError(status, "GET /releases")
                    return request(method, path, data)

                with patch.object(api, "request", side_effect=fail_once):
                    result = self.run_draft(api)
                self.assertEqual(result["commit"], COMMIT)
                self.assertEqual(self.delays, [2])
                self.assertEqual(len(api.assets), 6)
        self.assertIn("attempt 1/4", self.stderr.getvalue())
        self.assertIn("rechecking tag, draft and uploaded checksums", self.stderr.getvalue())

    def assert_accepted_write_recovered(self, operation):
        api = FakeApi(release=False, commit=None)
        request, upload = api.request, api.upload
        failed = False

        def request_then_fail(method, path, data=None):
            nonlocal failed
            result = request(method, path, data)
            if not failed and (method, path) == ("POST", operation):
                failed = True
                raise ApiError(504, "Accepted write lost its response")
            return result

        def upload_then_fail(release_id, name, data):
            nonlocal failed
            result = upload(release_id, name, data)
            if not failed and name == operation:
                failed = True
                raise ApiError(504, "Accepted upload lost its response")
            return result

        with patch.object(api, "request", side_effect=request_then_fail), \
                patch.object(api, "upload", side_effect=upload_then_fail):
            result = self.run_draft(api)
        self.assertTrue(failed)
        self.assertEqual(result["release_id"], "42")
        self.assertEqual(self.delays, [2])
        self.assertEqual(len(api.assets), 6)
        self.assertEqual(api.upload_attempts, 6)
        self.assertEqual(len(api.mutations), 8)
        self.assertEqual(sum(method == "POST" and path == "/git/refs"
                             for method, path, _ in api.calls), 1)
        self.assertEqual(sum(method == "POST" and path == "/releases"
                             for method, path, _ in api.calls), 1)
        self.assertFalse(any(method in ("PATCH", "DELETE") for method, _, _ in api.calls))

    def test_accepted_tag_with_lost_response_is_not_recreated(self):
        self.assert_accepted_write_recovered("/git/refs")

    def test_accepted_draft_with_lost_response_is_not_recreated(self):
        self.assert_accepted_write_recovered("/releases")

    def test_accepted_archive_with_lost_response_is_not_reuploaded(self):
        self.assert_accepted_write_recovered(self.items[0].name)

    def test_accepted_checksum_with_lost_response_is_not_reuploaded(self):
        self.assert_accepted_write_recovered(self.items[0].name + ".sha256")

    def test_transient_download_recovers_without_replacing_existing_assets(self):
        api = FakeApi()
        item = self.items[0]
        api.add_asset(item.name, item.data)
        api.add_asset(item.name + ".sha256", item.checksum_data)
        existing_ids = {name: asset["id"] for name, asset in api.assets.items()}
        download = api.download
        failed = False

        def fail_once(asset_id):
            nonlocal failed
            if not failed:
                failed = True
                raise TransientApiError("Interrupted asset download")
            return download(asset_id)

        with patch.object(api, "download", side_effect=fail_once):
            self.run_draft(api)
        self.assertEqual(self.delays, [2])
        self.assertEqual(api.upload_attempts, 4)
        self.assertEqual(len(api.assets), 6)
        for name, asset_id in existing_ids.items():
            self.assertEqual(api.assets[name]["id"], asset_id)

    def test_persistent_failure_stops_after_four_attempts(self):
        api = FakeApi()
        with patch.object(api, "request", side_effect=ApiError(504, "Unavailable")) as request:
            with self.assertRaisesRegex(ReleaseError, "exhausted after 4 attempts"):
                self.run_draft(api)
        self.assertEqual(request.call_count, 4)
        self.assertEqual(self.delays, [2, 4, 8])
        self.assertEqual(api.mutations, [])

    def test_permanent_http_failures_do_not_retry(self):
        for status in (400, 401, 403, 404, 409, 422, 429):
            with self.subTest(status=status):
                api = FakeApi()
                with patch.object(api, "request", side_effect=ApiError(status, "Rejected")) as request:
                    with self.assertRaises(ApiError):
                        self.run_draft(api)
                self.assertEqual(request.call_count, 1)
                self.assertEqual(self.delays, [])
                self.assertEqual(api.mutations, [])

    def test_corrupt_local_package_does_not_retry_or_contact_api(self):
        api = FakeApi()
        item = self.items[0]
        self.items[0] = type(item)(item.product, item.platform, item.name, item.data, b"bad checksum")
        with self.assertRaisesRegex(ReleaseError, "SHA256 mismatch"):
            self.run_draft(api)
        self.assertEqual(self.delays, [])
        self.assertEqual(api.calls, [])
        self.assertEqual(api.mutations, [])

    def test_moved_tag_during_retry_delay_stops_further_uploads(self):
        api = FakeApi()
        api.fail_upload_at = 2

        def move_tag(delay):
            self.delays.append(delay)
            api.commit = OTHER_COMMIT

        with self.assertRaisesRegex(ReleaseError, "different commit"):
            self.run_draft(api, sleep=move_tag)
        self.assertEqual(self.delays, [2])
        self.assertEqual(len(api.mutations), 1)
        self.assertEqual(len(api.assets), 1)

    def test_publication_during_retry_delay_stops_further_uploads(self):
        api = FakeApi()
        api.fail_upload_at = 2

        def publish(delay):
            self.delays.append(delay)
            api.release["draft"] = False

        with self.assertRaisesRegex(ReleaseError, "published or immutable"):
            self.run_draft(api, sleep=publish)
        self.assertEqual(self.delays, [2])
        self.assertEqual(len(api.mutations), 1)
        self.assertEqual(len(api.assets), 1)

    def test_starter_asset_stops_after_bounded_wait_without_deletion(self):
        api = FakeApi()
        item = self.items[0]
        api.add_asset(item.name, b"")
        api.assets[item.name]["state"] = "starter"
        with patch.object(api, "download") as download:
            with self.assertRaisesRegex(ReleaseError, "exhausted after 4 attempts.*state=starter"):
                self.run_draft(api)
        download.assert_not_called()
        self.assertEqual(self.delays, [2, 4, 8])
        self.assertEqual(api.assets[item.name]["state"], "starter")
        self.assertEqual(api.mutations, [])
        self.assertFalse(any(method == "DELETE" for method, _, _ in api.calls))

    def test_starter_that_completes_during_wait_is_verified_and_preserved(self):
        api = FakeApi()
        item = self.items[0]
        api.add_asset(item.name, b"")
        asset_id = api.assets[item.name]["id"]
        api.assets[item.name]["state"] = "starter"

        def finish_upload(delay):
            self.delays.append(delay)
            api.assets[item.name]["state"] = "uploaded"
            api.content[asset_id] = item.data

        self.run_draft(api, sleep=finish_upload)
        self.assertEqual(self.delays, [2])
        self.assertEqual(api.assets[item.name]["id"], asset_id)
        self.assertEqual(api.upload_attempts, 5)
        self.assertEqual(len(api.assets), 6)
        self.assertFalse(any(method == "DELETE" for method, _, _ in api.calls))

    def test_retry_after_is_respected_up_to_sixty_seconds(self):
        for advertised, expected in ((1, 2), (17, 17), (60, 60)):
            with self.subTest(retry_after=advertised):
                api = FakeApi()
                request = api.request
                failed = False
                self.delays.clear()

                def fail_once(method, path, data=None):
                    nonlocal failed
                    if not failed:
                        failed = True
                        raise ApiError(503, "Unavailable", retry_after=advertised)
                    return request(method, path, data)

                with patch.object(api, "request", side_effect=fail_once):
                    self.run_draft(api)
                self.assertEqual(self.delays, [expected])
                self.assertEqual(len(api.assets), 6)

    def test_retry_after_over_sixty_seconds_stops_without_an_early_retry(self):
        api = FakeApi()
        with patch.object(api, "request", side_effect=ApiError(503, "Unavailable", retry_after=61)) as request:
            with self.assertRaisesRegex(ReleaseError, "61s retry delay, beyond automatic recovery"):
                self.run_draft(api)
        self.assertEqual(request.call_count, 1)
        self.assertEqual(self.delays, [])
        self.assertEqual(api.mutations, [])

    def test_draft_cli_recovers_a_timeout_and_writes_github_outputs(self):
        api = FakeApi()
        request = api.request
        failed = False

        def fail_once(method, path, data=None):
            nonlocal failed
            if not failed:
                failed = True
                raise ApiError(504, "GET /releases")
            return request(method, path, data)

        def retry_without_sleep(*args, **kwargs):
            return prepare_draft_with_retries(*args, **kwargs, sleep=self.delays.append)

        with TemporaryDirectory() as temporary:
            output = Path(temporary) / "github-output.txt"
            package_directory = Path(temporary) / "packages"
            argv = ["release_pipeline.py", "draft", "--tag", "v1.2.3", "--commit", COMMIT,
                    "--prerelease", "false", "--package-directory", str(package_directory),
                    "--output", str(output)]
            with patch.object(sys, "argv", argv), \
                    patch.dict(os.environ, {"GH_REPO": "test/repo", "GH_TOKEN": "unit-test-token"}), \
                    patch("release_pipeline.load_packages", return_value=self.items) as load, \
                    patch("release_pipeline.release_products", return_value=EXPECTED_PAIRS), \
                    patch("release_support.git", return_value=COMMIT), \
                    patch("release_pipeline.GitHubApi", return_value=api), \
                    patch.object(api, "request", side_effect=fail_once), \
                    patch("release_pipeline.prepare_draft_with_retries", side_effect=retry_without_sleep) as recovery:
                main()
            load.assert_called_once_with(package_directory, COMMIT, EXPECTED_PAIRS)
            recovery.assert_called_once_with(api, "v1.2.3", COMMIT, False, self.items, expected_pairs=EXPECTED_PAIRS)
            self.assertEqual(output.read_text(encoding="utf-8").splitlines(), [
                "release_id=42", "release_url=" + api.release["html_url"],
                "tag=v1.2.3", "commit=" + COMMIT])
        self.assertEqual(self.delays, [2])
        self.assertEqual(len(api.assets), 6)
        self.assertNotIn("unit-test-token", self.stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
