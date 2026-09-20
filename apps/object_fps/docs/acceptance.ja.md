# Object_FPS：配布 package と GPU の検証

[繁體中文](acceptance.zh-Hant.md) · [共通の描画設計](../../../docs/rendering_architecture.ja.md) · [バージョン公開](../../../docs/releasing.ja.md)

## 1. App が所有する契約

Object_FPS の `CMakeLists.txt` が SDL_GPU、input、image、ttf、ufbx を要求し、自身の asset、shader、install、検証コマンドを登録します。Engine の `config/engine/projects.csv` で app とターゲットを有効にします。共通 CI は生成済み package manifest を読み、ゲーム名やゲームプレイの規則を持ちません。

```sh
cmake --preset dev -DGYO_APPS=object_fps -DGYO_BUILD_UI_EDITOR=OFF
cmake --build --preset dev
ctest --preset dev
cmake --install build/dev --prefix /absolute/path/to/stage
```

Package は `bin/gyo_object_fps`、`bin/assets/common`、`bin/assets/object_fps`、`bin/shaders/builtin`、`bin/shaders/object_fps`、必要な動的ライブラリ、生成した `share/gyo/apps/object_fps/manifest.json`、app の検証ツールを含みます。UI editor は含めません。Archive は `gyo-object_fps-<platform>.tar.gz`、ルートは `gyo-object_fps` です。

`--validate-package` はウィンドウや GPU を作らず、配布 asset、shader bundle、読み込みを検査します。Release の app 固有チェックは独立した場所から完全な package を実行し、common asset、内蔵 shader manifest、ゲーム shader manifest を順に除去して、各欠落を正しく拒否することを確認します。Source tree からの補完は許可しません。App の宣言が GPU を必要とするため `GYO_RENDER_DEVICE=NONE` は配置時のエラーです。Headless 検証は実行モードであり、コンパイル依存関係を省略する仕組みではありません。

## 2. Quick、Release、実機の境界

| 検証 | Platform | 実際の範囲 |
|---|---|---|
| `--startup-smoke-test` | Windows／Linux／macOS、quick と Release | 別の作業ディレクトリーから配布版を起動し、catalog／campaign／FBX モデルの構成を実際に読み込む。ウィンドウと GPU は作らない |
| `manual_gpu_smoke.py --suite quick` | Linux、quick CI | Xvfb の表示環境で Mesa Lavapipe Vulkan を強制選択し、カスタム shader の赤／青描画と読み戻しを1回実行 |
| `--headless-smoke-test` | Windows／Linux／macOS、Release のみ | メニュー、開始、取り出し、ジャンプ、一時停止／再開、着地、1回の射撃、リロード完了も検査。ウィンドウと GPU は作らない |
| `manual_gpu_smoke.py --suite full` | Linux、Release のみ | shader、世界、メニュー、武器、リロード、3比率の銃口診断、計8件 |
| 物理 GPU／対話操作 | 各プラットフォームの実機 | 全8件の GPU 診断と手動操作。Windows／macOS の hosted runner はこの項目の合格を主張しない |

Linux で Lavapipe がない場合、device の生成失敗、描画失敗、タイムアウトはいずれも job の失敗になります。「GPU がなければスキップして成功」とする経路はありません。Helper はログに記録された実際の driver／shader 形式が要求と一致することも確認し、`summary.json` に結果を保存します。`vulkan-info.log` は ICD と Vulkan device 情報を記録します。これでソフトウェア Vulkan の描画経路を確認し、物理 GPU は別途検証します。

共通の検証フローが3プラットフォームの matrix を集約し、必須 job が失敗、キャンセル、スキップした場合は Draft 作成へ進みません。Quick 経路では設計どおり Release 専用の重い処理を実行せず、Summary にモードと未実行項目を明記します。完全な検証済みとは扱いません。Branch protection には、quick フローで実際に表示される集約チェック名を指定してください。

## 3. ダウンロード後の実機確認

Actions artifacts または Release から対応 package をダウンロードし、tar.gz を展開して、グラフィカルデスクトップのある実機で実行します。

```sh
python manual_gpu_smoke.py --package /absolute/path/to/gyo-object_fps \
  --driver vulkan --suite full --output /absolute/path/to/diagnostics
```

macOS は `--driver metal`、Windows は `--driver d3d12` と `--driver vulkan` をそれぞれ確認します。Python helper はカスタム shader の赤／青ピクセル読み戻し、世界、メニュー、武器、Reload 全体、16:9／4:3／21:9 の銃口投影 smoke を実行し、終了コード、ログ、診断画像を保存します。各項目の制限は 120 秒です。Python がない場合は `bin/gyo_object_fps --gpu-driver metal --muzzle-smoke-test --capture-dir /absolute/path/to/captures` などを直接実行できます。

`--suite full` はデフォルトの全8件、`--suite quick` は shader 描画1件です。`--suite ci` は shader、world、menu の3件を手動で選ぶために残しています。`--timeout` は各項目の秒数上限です。1件でも失敗すると helper は非ゼロで終了しますが、残りのケースも実行して結果を保存します。

制限環境で Python の一時ディレクトリーを利用できない場合、GPU helper に `--work-directory /absolute/path/to/new-work` を指定できます。このディレクトリーは未作成である必要があり、検証後も調査用に残ります。`--package` の外側を指定します。

手動ではマウス／キーボード、Space ジャンプ、R リロード、H 収納／取り出し、指と銃の遮蔽、壁際の射撃、exposure と HUD、リサイズ／最小化を確認します。UI editor は別のツールビルドで確認します。OS、CPU アーキテクチャ、GPU／ドライバー、package の commit、要求した／実際のバックエンド、`summary.json` を添えて報告すると、ビルド・データ・GPU のどの問題かを区別できます。

実機でソースからビルドした場合は `ctest --test-dir build/dev -L gpu --output-on-failure` で engine＋game の GPU テスト全体を実行できます。独立した `render.sdl_gpu_mesh_smoke` は、UV／部分矩形、深度とカリング、sRGB／線形色、alpha、非対称行列、13×7 後処理も読み戻し値で検証します。このテスト executable は Object_FPS のダウンロード package には含めません。

ソースの helper は `apps/object_fps/ci/manual_gpu_smoke.py`、インストール済み package のルートにも `manual_gpu_smoke.py` を置きます。上記の script をその絶対パスに置き換えて実行できます。実機の結果と CI のソフトウェア Vulkan の証拠を分けて記録します。[以前の検証結果](../../../docs/rendering_architecture.ja.md#r10) は新しい registry／matrix の hosted CI 合格を意味しません。

