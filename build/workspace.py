"""Temporary workspaces with inherited Windows ACLs, including restricted CI tokens."""

from pathlib import Path
import shutil
import tempfile
import uuid


class TemporaryDirectory:
    def __init__(self, prefix="gyo-ci-"):
        # Python 3.13's mode=0700 creates an owner-only Windows ACL that can
        # exclude the restricted token running the same process. Normal mkdir
        # inherits the user's temporary directory permissions instead.
        self.parent = Path(tempfile.gettempdir()).resolve()
        self.path = self.parent / (prefix + uuid.uuid4().hex)
        self.path.mkdir()
        self.name = str(self.path)

    def cleanup(self):
        target = self.path.resolve()
        if target.parent != self.parent or target.name != self.path.name:
            raise RuntimeError("Refusing cleanup outside this temporary workspace")
        if target.exists():
            shutil.rmtree(target)

    def __enter__(self):
        return self.name

    def __exit__(self, *args):
        self.cleanup()
