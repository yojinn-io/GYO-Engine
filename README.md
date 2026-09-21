[English](README.en.md) · [繁體中文](README.zh-Hant.md)

# GYO-Engine

GYO は C++20 のゲームエンジンです。Runtime、Asset、Input、Collision、Model、Text、Render、UI の責務を分け、エンジンライブラリをゲームと設計ツールへ静的リンクします。ゲームのルールとコンテンツはゲーム側が所有し、エンジンは特定のゲームへ依存しません。

## ディレクトリと責務

| ディレクトリ | 責務 |
|---|---|
| `apps/` | ゲームのコードと最小限のプロジェクト宣言 |
| `assets/<game>/` | 準備済み runtime コンテンツ、catalog、`content.json` |
| `engine/` | エンジン全モジュール、adapter、`config/projects.csv` |
| `tools/` | 設計支援。現在は UI editor |
| `tests/common`、`tests/<project>` | エンジン共通とプロジェクト固有の検証 |
| `build/cmake`、`build/ci` | ビルド、統合、パッケージ、受入検証の支援 |
| `build/target/` | Git 管理外のビルドツリー、実行可能な製品、ログ |
| `docs/`、`third_party/` | 設計文書と固定依存パッケージの wrapper |

Shader compiler は `engine/render/shaders/pipeline` が所有します。ルートの `tools/` は authoring 用です。`build/` 全体を削除しないでください。管理対象の開発支援コードが含まれます。

## ゲームをビルドする

CMake 3.30 以降、C++20 compiler、Ninja を使用します。Windows は x64 MSVC 開発環境を初期化してください。資産組立には Python 3、shader にはエンジン管理の native host compiler を使用します。依存ソースは build tree に取得し、既存 source cache は明示的に再利用できます。

`engine/config/projects.csv` でゲームと対象 OS を有効にし、リポジトリのルートから実行します。

```sh
cmake --preset dev -DGYO_APPS=object_fps -DGYO_BUILD_UI_EDITOR=OFF
cmake --build --preset dev --target gyo_object_fps
```

ビルド状態は `build/target/_build/dev`、実行可能なゲームは `build/target/object_fps/bin` です。ゲームは実行ファイル相対の `assets/object_fps` だけを読み、内蔵／ゲーム shader もその下に置きます。配布製品の実行にソースツリーは不要です。

通常ビルドでは tests と CI/package acceptance を無効にしています。ゲームは tests、CI コード、editor に依存しません。Backend-neutral engine の検証は次の通りです。

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

ゲームの検証には `test` preset を使用します。GPU 検証には対応する描画環境が必要で、CPU/CLI 検証と結果を分けます。過去のログは新 revision の成功証拠ではありません。

## 設計ツールと資産

```sh
cmake --preset dev -DGYO_APPS= -DGYO_BUILD_UI_EDITOR=ON
cmake --build --preset dev --target gyo_ui_editor
```

GUI editor は `build/target/toolchain/bin` に組み立てます。読み取り専用 catalog を使って `gyo.ui` JSON を編集します。利用者が選択したソース資産をゲーム資産に準備・登録し、その後は共通 build hook が `content.json` に従って資産組立と shader compile を自動実行します。本機と CI は同じ経路を使い、ゲーム CMake に資産ルールを書きません。

派生ゲームは `apps/<game>` と `assets/<game>` を手動コピーして CSV 行を追加します。CI、tests、editor、元の art archive はゲームに含めません。[作成／コピー手順](docs/creating_apps.md)を参照してください。

## 統合と公開

ゲーム選択は registry に従い、ディレクトリ数から推測しません。選択されたゲームが失敗すると Engine 統合全体が失敗します。各対応プラットフォームは、エンジンをリンクした GUI UI editor を含む固定 toolchain archive を生成し、有効なゲームは別 archive を生成します。ゲーム 0 件でも公開できます。配布物は実行ファイルと必要な依存物であり、Engine SDK やソースコードのパッケージではありません。

`Prepare Release` は commit を固定し、全製品をビルド・検証して tag と Draft Release を準備します。Draft の公開は利用者の別操作です。ゲーム包に入るのはゲーム、資産、runtime 依存物だけで、検証ツールは外側から実行します。

## 設計資料

- [全体構造と責務](docs/architecture.md)
- [UI 標準と編集手順](docs/ui_toolchain.md)
- [描画と shader 契約](docs/rendering_architecture.ja.md)
- [3D 資産とアニメーションの制約](docs/architecture/3d-assets.md)
- [公開手順](docs/releasing.ja.md)

`Object_FPS` は campaign、UI、grid combat、CPU skinning の一人称武器、SDL_GPU を検証する取り外し可能な利用側です。汎用ゲーム framework を定義しません。一般的な scene 管理、完全な物理、GPU skinning、PBR、scripting、動的 plugin ABI は現在の実装範囲外です。
