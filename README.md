[English](README.en.md) · [繁體中文](README.zh-Hant.md)

# GYO-Engine

GYO は C++20 のゲームエンジンです。Base、Runtime、Asset、Math、Time、Threads、Trace、Net、Input、Collision、Model、Text、Render、UI の責務を分け、エンジンライブラリをゲームと設計ツールへ静的リンクします。ゲームのルールとコンテンツはゲーム側が所有し、エンジンは特定のゲームへ依存しません。

## ディレクトリと責務

| ディレクトリ | 責務 |
|---|---|
| `apps/` | ゲームのコードと最小限のプロジェクト宣言 |
| `assets/<game>/` | 準備済み runtime コンテンツ、catalog、`content.json` |
| `engine/` | エンジン全モジュール、adapter、`config/projects.csv` と `config/tools.csv` |
| `tools/` | 設計支援。UI editor とローカル専用のゲーム preview（現在は無効） |
| `services/` | エンジンが所有する再利用可能な Go service 機構（`gyo_gateway`）。ゲーム固有の service は `apps/<game>/` に置く |
| `tests/common`、`tests/<project>` | エンジン共通とプロジェクト固有の検証 |
| `build/cmake`、`build/ci` | ビルド、統合、パッケージ、受け入れ検証の支援 |
| `build/acceptance/` | 製品の外側から実行する共通およびプロジェクト固有の受け入れ検証ツール |
| `build/target/` | Git 管理外のビルドツリー、実行可能な製品、ログ |
| `docs/`、`third_party/` | 設計文書と固定依存パッケージの wrapper |

Shader compiler は `engine/render/shaders/pipeline` が所有します。ルートの `tools/` は authoring 用です。`build/` 全体を削除しないでください。管理対象の開発支援コードが含まれます。

## ゲームをビルドする

CMake 3.30 以降、C++20 compiler、Ninja を使用します。Windows は x64 MSVC 開発環境を初期化してください。アセットの組み立てには Python 3、shader にはエンジン管理の native host compiler を使用します。依存ソースは build tree に取得し、既存 source cache は明示的に再利用できます。

`engine/config/projects.csv` でゲームと対象 OS を有効にし、リポジトリのルートから実行します。

```sh
cmake --preset dev -DGYO_APPS=object_fps_pvp -DGYO_TOOLS=
cmake --build --preset dev --target gyo_object_fps_pvp
```

ビルド状態は `build/target/_build/dev`、実行可能なゲームは `build/target/object_fps_pvp/bin` です。ゲームは実行ファイル相対の `assets/object_fps_pvp` だけを読み、組み込み／ゲーム shader もその下に置きます。配布製品の実行にソースツリーは不要です。

通常ビルドでは tests と CI/package acceptance を無効にしています。ゲームは tests、CI コード、editor に依存しません。Backend-neutral engine の検証は次の通りです。

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

ゲームの検証には `test` preset を使用します。GPU 検証には対応する描画環境が必要で、CPU/CLI 検証と結果を分けます。過去のログは新 revision の成功証拠ではありません。

## 設計ツールとアセット

```sh
cmake --preset dev -DGYO_APPS= -DGYO_TOOLS=ui_editor
cmake --build --preset dev --target gyo_ui_editor
```

GUI editor は `build/target/toolchain/bin` に組み立てます。読み取り専用 catalog を使って `gyo.ui` JSON を編集します。利用者が選択したソースアセットをゲームアセットに準備・登録し、その後は共通 build hook が `content.json` に従ってアセットの組み立てと shader compile を自動実行します。ローカルと CI は同じ経路を使い、ゲーム CMake にアセットのルールを書きません。

派生ゲームは `apps/<game>` と `assets/<game>` を手動コピーして CSV 行を追加します。CI、tests、editor、元の art archive はゲームに含めません。[作成／コピー手順](docs/creating_apps.md)を参照してください。

## 統合と公開

ゲーム選択は `projects.csv`、ツール選択は `tools.csv` に従い、ディレクトリ数から推測しません。各対応プラットフォームは release 選択されたツール（現在は UI Editor GUI）の toolchain archive を生成し、有効なゲームは別 archive を生成します。選択されたゲーム、ツール、必須検証の失敗は Engine 統合全体を失敗させます。ゲーム 0 件でも公開できます。配布物は実行ファイルと必要な依存物であり、Engine SDK やソースコードのパッケージではありません。

`Prepare Release` は公開ライン（`tools` または各ゲーム）を一つ選んで commit を固定し、その製品をビルド・検証して tag と Draft Release を準備します。Draft の公開は利用者の別操作です。ゲームパッケージに入るのはゲーム、アセット、runtime 依存物だけで、検証ツールは外側から実行します。

## 設計資料

- [全体構造と責務](docs/architecture.md)
- [UI 標準と編集手順](docs/ui_toolchain.md)
- [ツール登録・選択と旧 CMake 設定の移行](docs/tool_projects.md)
- [数学基盤の契約](docs/architecture/math.md)
- [エラー処理の契約（Result／Error／Assert）](docs/architecture/error-handling.md)
- [トレース転送とログの契約](docs/architecture/trace.md)
- [描画と shader 契約](docs/rendering_architecture.ja.md)
- [3D アセットとアニメーションの制約](docs/architecture/3d-assets.md)
- [公開手順](docs/releasing.ja.md)

`apps/` 配下の各ゲームは独立したプロジェクトであり、エンジンはそのいずれにも依存せず、汎用ゲーム framework も定義しません。現在有効なのは `object_fps_pvp`（サーバー権威の LAN 対戦。[ネットワーク構成](docs/object_fps_pvp/network-architecture.zh-Hant.md)）です。`object_fps` と `object_fps_v2` は無効化されており、Engine の基盤統一（Math、Result、include パス、入力層、Collision）の後は移行してから再有効化する必要があります。移行一覧は `docs/architecture/plans/` 配下の各計画の `inactive_products.md` です。一般的な scene 管理、完全な物理、GPU skinning、PBR、scripting、動的 plugin ABI は現在の実装範囲外です。
