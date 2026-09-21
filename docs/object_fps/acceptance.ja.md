# Object_FPS：外部製品検証と GPU 検査

[繁體中文](acceptance.zh-Hant.md) · [描画設計](../rendering_architecture.ja.md) · [公開手順](../releasing.ja.md)

## 1. ゲームと検証を分ける

`apps/object_fps` はゲームコード、metadata、source declarations だけを所有します。単独ゲーム build に tests、CI、packaging、設計ツールは不要です。`tests/object_fps` が unit tests、`build/acceptance/object_fps` が独立 `gyo_object_fps_acceptance` を所有し、製品 executable に診断 source や test macro を注入しません。対話的 viewmodel preview は `tools/object_fps_preview` の `gyo_object_fps_preview` が担当し、ゲームの起動モードではありません。

共通 runner は `build/acceptance/common`、Object_FPS の GPU／内容検証は `build/acceptance/object_fps` にあります。正式 game archive にこれらの Python scripts や probe を含めません。

`engine/config/projects.csv` でゲームと対象 OS を有効にして、検証を明示的に選びます。

```sh
cmake --preset test -DGYO_APPS=object_fps -DGYO_BUILD_UI_EDITOR=OFF -DGYO_ENABLE_PACKAGING=ON
cmake --build --preset test
ctest --preset test
cmake --install build/target/_build/test --prefix build/target/acceptance-stage/object_fps --component object_fps
```

製品は `build/target/object_fps`、probe は `build/target/_build/test/acceptance/object_fps/bin` です。全資産は `bin/assets/object_fps` にまとめ、catalog、data、model、font、`shaders/builtin`、`shaders/game` を含みます。Build-only shader source 情報は配布 descriptor に残さず、不足時に source tree へ fallback しません。

Product manifest は `share/gyo/products/object_fps/manifest.json` です。外部 runner は identity、executables、checks を読み、元ゲーム名から推測しません。複製では deployment identity が変わっても内部 AssetId／shader ID を保持できます。[ゲーム作成／コピー](../creating_apps.md#manual-copy)も参照してください。

## 2. 検証を実行する

以下は前節の install で作った独立検証製品（generated product manifest を含む）を Windows で検査する例です。`FULL_COMMIT_SHA` を対象の完全な commit に置き換え、他 OS では platform 名を変えます。

```sh
python build/acceptance/common/run_package_checks.py --stage build/target/acceptance-stage/object_fps --product object_fps --platform windows-x64 --revision FULL_COMMIT_SHA --profile release --logs build/target/acceptance/object_fps --probe-directory build/target/_build/test/acceptance/object_fps/bin
```

Runner は外部 working directory から検証します。必要な時は隔離コピーの bin に probe を一時配置し、ゲームと同じ資産パスを使用します。異常、timeout、必要証拠の欠落は失敗であり、probe は正式 archive に入りません。

| 検査 | 範囲 |
|---|---|
| Startup／内容ロード | Catalog、campaign、model、compiled shader。GPU 不要 |
| Headless gameplay | Menu、開始、移動／jump、pause、shoot、reload 状態 |
| Package negative cases | 必須資産／shader を外して明確な失敗を確認。Checkout 読込は禁止 |
| GPU quick | 宣言された platform の限定 shader 描画／readback |
| GPU release／manual | World、menu、weapon、reload、shader、複数比率の muzzle 検証 |

以前の `--startup-smoke-test`、`--headless-smoke-test`、`--validate-package`、GPU probe switches は acceptance executable が所有します。ゲーム製品へ渡さないでください。通常のゲームは一般操作、`--help`、`--gpu-driver` を持ちます。

## 3. GPU と実機

現在の GPU release check は Linux に宣言されています。同じ profile の非 GPU 検査後、同じ logs に `--gpu --driver vulkan` を加えます。Common runner は契約の platform/profile だけを実行するため、空の GPU check 集合を実機成功とは扱いません。

Windows／macOS の手動検証には `build/acceptance/object_fps/manual_gpu_smoke.py` を使います。Install 済み製品を独立検証ディレクトリへコピーし、その bin に対応 probe と必要 runtime libraries を配置して、`--package` と `--probe` を渡します。Windows は `--driver d3d12`／`vulkan`、macOS は `metal` です。`--suite full` で8項目、`--output` で報告先を指定します。Probe を正式製品や archive に戻さないでください。

Linux CI の Xvfb／Lavapipe は software Vulkan の検証です。物理 GPU や Windows/macOS hosted の実機画面を証明しません。要求した GPU 能力不足、driver／shader format の不一致、timeout、必要画像の欠落は失敗として扱います。

実機では mouse／keyboard、Space jump、R reload、H holster/draw、手と銃の遮蔽、壁際での射撃、exposure と HUD を確認します。OS、GPU／driver、product revision、要求／実際の renderer を記録し、summary と画像を残します。

Engine の GPU 検査は `tests/common` にあり、frame lifecycle、mesh 更新、UV、depth/culling、color/alpha、matrix、post process を検証します。ゲーム archive には含めません。[描画文書の過去の結果](../rendering_architecture.ja.md#r10)は当時の証拠であり、現在の revision が成功したことを意味しません。
