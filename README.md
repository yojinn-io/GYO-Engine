[日本語](README.md) | [繁體中文](README.zh-Hant.md) | [English](README.en.md)

# GYO-Engine

GYO は**再利用可能な C++ ゲームランタイム**です。SDL、OS API、グラフィックス API の機能を、再利用できるゲームの機構へ変換します。ゲームが方針とコンテンツを提供し、任意の外部コントローラーは中立な公開境界を通してランタイムを観察・操作します。

巨大なエディター環境や、小型の Godot、Unity、Unreal を目指してはいません。ツールは具体的なランタイムデータ標準を扱う独立した実行ファイルです。将来の汎用性を先取りするのではなく、実際のゲームで必要になった責務から育てます。

`apps/object_fps` は現在のリポジトリー内の適合性検証用ゲームです。GYO の入力 action、アセット識別・読み込み、描画 submission／device 契約、ライフサイクル、型付きランタイム境界を直接利用し、骨格が実用的か確認します。検証対象の標準は GYO であり、GYO が Object_FPS 固有の方針やデータに依存することはありません。

責務と依存方向は [docs/architecture.md](docs/architecture.md) を参照してください。描画経路、共通 HLSL、shader ABI、配布とネイティブ検証は [日本語](docs/rendering_architecture.ja.md)・[繁體中文](docs/rendering_architecture.zh-Hant.md)で説明しています。

## 実装状態の読み方

- `[実装済み]`：今回の適合性検証以前から存在するコード。
- `[今回の到達点]`：Object_FPS で骨格を検証する過程で追加・修正した、範囲の定まったコード。
- `[必要時に追加]`：設計上の方向性のみ。ディレクトリーや型が存在するとは限らず、具体的な責務が生じるまで作成しません。

以下は**実装状態を明示したアーキテクチャの構想**です。表示するすべてのディレクトリーや機能が実装済みという意味ではありません。

## アーキテクチャの構想

```text
GYO-Engine/
├─ CMakeLists.txt
├─ README.md                                      日本語入口、英語・繁体字版を併設
├─ config/engine/projects.csv                     app 選択、個人用メタデータ
├─ docs/
│  └─ architecture.md                            [今回の到達点]
│
├─ third_party/                                  依存ライブラリのラッパー
│  ├─ sdl3/                                      [実装済み]
│  ├─ sdl3_image/                                [今回の到達点; 任意の PNG デコード]
│  ├─ sdl3_ttf/                                  [今回の到達点; 任意のフォントラスタライズ]
│  ├─ nlohmann_json/                             [実装済み; アセットカタログ]
│  ├─ ufbx/                                      [今回の到達点; 任意の FBX ローダー]
│  ├─ imgui/                                     [実装済み; エディターの操作 UI]
│  └─ doctest/                                   [実装済み; テスト専用]
│
├─ engine/                                       再利用可能な、バックエンドに依存しない機構
│  ├─ include/engine/
│  │  ├─ base/                                   [実装済み]
│  │  ├─ io/                                     [実装済み]
│  │  ├─ asset/                                  [実装済み + 今回の到達点]
│  │  │  ├─ catalog/                             識別子とパスのメタデータ
│  │  │  ├─ core/                                レコード、ハンドル、キャッシュ、寿命
│  │  │  ├─ loading/
│  │  │  │  ├─ IAssetSource.hpp
│  │  │  │  ├─ NativeFileAssetSource.hpp         [今回の到達点]
│  │  │  │  ├─ LoaderRegistry.hpp
│  │  │  │  └─ AssetPipeline.hpp
│  │  │  └─ loaders/
│  │  │     ├─ FontAsset.hpp                     [今回の到達点; エンコードされたフォントバイト列]
│  │  │     ├─ FontLoader.hpp                    [実装済み; バイト列 -> FontAsset]
│  │  │     ├─ TextureAsset.hpp                  CPU 上でデコードしたピクセル
│  │  │     └─ sdl_image/                        [今回の到達点; 任意のローダー]
│  │  └─ runtime/                                [今回の到達点]
│  │     ├─ FrameContext.hpp
│  │     ├─ IRuntimeClient.hpp
│  │     ├─ IRuntimePort.hpp                     型付き Query/Command/Event 境界
│  │     ├─ RuntimeControl.hpp
│  │     └─ RuntimeLoop.hpp
│  └─ src/
│     ├─ io/                                     [実装済み]
│     ├─ asset/                                  [実装済み + 今回の到達点]
│     └─ runtime/RuntimeLoop.cpp                  [今回の到達点]
│
├─ platform/                                     OS・ウィンドウ・イベントのアダプター
│  ├─ sdl/                                       [今回の到達点]
│  └─ win32/                                     [必要時に追加; 未作成]
│
├─ input/                                        入力機構、systems/ に混在させない
│  ├─ include/engine/input/                      [今回の到達点]
│  │  ├─ PhysicalInputFrame.hpp
│  │  └─ InputActionMap.hpp
│  ├─ backend/sdl/                               [今回の到達点]
│  │  └─ SdlInput                                SDL イベント -> 物理入力
│  └─ tests/                                     [今回の到達点]
│
├─ text/                                         最小限のフォント・テキスト機構
│  ├─ include/text/                              [今回の到達点; バックエンド非依存]
│  │  ├─ ITextRasterizer.hpp                     フォントバイト列 + UTF-8 文字列 -> RGBA8 ビットマップ
│  │  ├─ TextTypes.hpp                          要求と所有権を持つ CPU ビットマップ
│  │  └─ TextError.hpp
│  └─ backend/sdl_ttf/                           [今回の到達点; 任意のアダプター]
│
├─ model/                                        [今回の到達点; CPU モデル・アニメーション・スキニング]
│  └─ backend/ufbx/                              [任意; FBX バイト列 -> ModelAsset]
├─ collision/                                    [今回の到達点; カプセル・レイ・球の問い合わせ]
├─ render/                                       レンダラーの契約と実装
│  ├─ include/render/                            [今回の到達点; バックエンド非依存]
│  │  ├─ RenderQueue.hpp
│  │  ├─ IRenderDevice.hpp
│  │  ├─ RenderHandle.hpp
│  │  └─ RenderTypes.hpp
│  ├─ backend/
│  │  ├─ sdl/                                    [今回の到達点; クリア・表示アダプター]
│  │  ├─ sdl_gpu/                                [今回の到達点; 任意の SDL_GPU デバイス]
│  │  ├─ dx12/                                   [必要時に追加; 未作成]
│  │  ├─ vulkan/                                 [必要時に追加; 未作成]
│  │  └─ opengl/                                 [必要時に追加; 未作成]
│  ├─ shaders/                                   共通 HLSL ABI と内蔵 bundle
│  └─ tests/                                     [今回の到達点]
│
├─ ui/                                           [今回の到達点; 閉じた JSON UI v1 標準]
│  ├─ include/ui/                                文書・codec・runtime・renderer の境界
│  ├─ src/                                       レイアウト、binding、focus、draw-list 連携
│  └─ tests/                                     codec・runtime・renderer の検証
│
├─ framework/                                    再利用可能なゲームドメイン方針 [必要時に追加]
│  ├─ stage/
│  ├─ combat/
│  └─ ai/
│
├─ apps/                                         構成ルートと具体的なゲーム
│  ├─ runtime/                                   [今回の到達点] 最小限のクリア・表示 app
│  └─ object_fps/                                [今回の到達点; 現行の適合性検証用ゲーム]
│     ├─ include/RetroFPS/                       Object_FPS の方針・ドメイン型
│     ├─ src/                                    アダプターは GYO の契約を直接利用
│     │  └─ App/ObjectFpsUi.cpp                  ゲーム binding・action と C++ HUD 方針
│     └─ tests/                                  ゲーム方針・UI command の headless テスト
│
├─ assets/                                       ゲームが所有する実行時コンテンツ
│  ├─ common/                                    [今回の到達点; 独立したルートの共通プリミティブ]
│  ├─ object_fps/                                [今回の到達点]
│  │  ├─ asset_catalog.json
│  │  ├─ data/                                   キャンペーン・敵・武器の定義
│  │  ├─ fonts/                                  ゲーム選択の UI フォントとライセンス
│  │  ├─ maps/                                   現在の適合性検証データ
│  │  └─ textures/
│  └─ <game_id>/                                 [必要時に追加; ゲームごとに分離したルート]
│
├─ tools/                                        独立した任意の実行ファイル
│  └─ editor/                                    [今回の到達点; JSON UI オーサリングツール]
│
└─ tests/
   ├─ engine_tests/                              [実装済み + 今回の到達点]
   └─ backend integration tests/                 [必要時に追加]
```

汎用的な `systems/` の入れ物は設けません。入力は `input/`、モデルアニメーションは `model/`、幾何学的な問い合わせは `collision/` に置きます。将来の audio、完全な physics、navigation も、実装する時点で固有のモジュールにします。

`apps/<game_id>` と `assets/<game_id>` は対です。Object_FPS は `apps/object_fps` と `assets/object_fps` を使用し、カタログや内容を共通のゲームアセット置き場へ混在させません。

## 現在の骨格をビルドする

CMake 3.30 以上と C++20 コンパイラーが必要です。初回の構成時に、有効な外部依存のソースをビルドディレクトリーへ取得する場合があります。新しい Visual Studio generator には新しい CMake が必要な場合がありますが、動作している CLion／Ninja の MSVC profile は既存の選択を維持できます。ネイティブ preset は Ninja、Windows では x64 MSVC 開発者シェルも使用します。

[config/engine/projects.csv](config/engine/projects.csv) がローカルと CI の共通 app 選択リストです。列は `name,description,version,enabled,windows,linux,macos`。名前はリポジトリールートの `apps/<name>` に対応します。説明と版番号は個人の記録であり、ビルド、package 名、Release tag を変更しません。真偽値は `1/0` と `true/false` を受け付け、`enabled` と**ターゲットプラットフォーム**の両方が有効な場合だけ選択します。完全な CSV・依存関係の契約は[ビルド設計](docs/architecture.md#build-project-management)を参照してください。

CSV で選択した app と独立した UI editor をビルドします。

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --install build/dev --prefix /absolute/path/to/stage
```

App、tool、任意の adapter を含めず、中立モジュールとテストをビルドします。

```sh
cmake --preset core
cmake --build --preset core
ctest --preset core
```

対象を絞る場合は `-DGYO_APPS=object_fps`、複数なら `"-DGYO_APPS=object_fps;runtime"` のように引用したセミコロンリストを渡します。部分集合の指定なので、まず CSV で対象 app とターゲットを有効にします。無効な行を上書きする指定ではありません。Editor だけの構成は次のとおりです。

```sh
cmake -S . -B build-ui-editor -DGYO_APPS= -DGYO_BUILD_UI_EDITOR=ON
cmake --build build-ui-editor --config Debug --target gyo_ui_editor
```

既存の engine 能力を使う app の追加・削除は、自身のディレクトリーと CSV だけを変更します。App の `CMakeLists.txt` が要求、target、asset、install、配布検証を宣言し、共通モジュールや CI は app 名の一覧を持ちません。CSV 変更は再構成を起動し、要求を再計算します。以前の app 別ビルドオプションは廃止し、古い cache には移行方法を表示します。新しいビルドツリーを使うか、指示された旧 cache 項目を削除して CSV と `GYO_APPS` に移行してください。

| オプション | 既定値 | 作用 |
|---|---:|---|
| `GYO_APPS` | `AUTO` | CSV とターゲットから app を選択。空値は app なし、セミコロンリストは部分集合。 |
| `GYO_BUILD_UI_EDITOR` | `ON` | 独立した SDLRenderer／ImGui UI エディターをビルド。`GYO_APPS=` で editor のみ。 |
| `GYO_BUILD_SDL_GPU_BACKEND` | `OFF` | `IRenderDevice` の SDL_GPU 実装を明示要求。App の要求からも選択。 |
| `GYO_RENDER_DEVICE` | `AUTO` | `AUTO`、`SDL_GPU`、`NONE`。GPU 必須 app と `NONE` は構成エラー。 |
| `GYO_GPU_DRIVER` | `AUTO` | 実行時方針は `AUTO`、`D3D12`、`VULKAN`、`METAL`。不適合なターゲットを構成時に拒否。 |
| `GYO_SHADER_BUNDLE` | `AUTO` | ターゲットの shader 形式。`"DXIL;SPIRV"` などのリストで配布ドライバーを制限。 |
| `GYO_SHADER_TOOL_EXECUTABLE` | 未設定 | ネイティブ host shader ツールの絶対パス。クロスコンパイルでは必須。 |
| `GYO_MSVC_REDIST_DIR` | 未設定 | MSVC 再配布ファイルのルート。省略時は選択コンパイラーの場所から検出。 |
| `GYO_BUILD_SDL_IMAGE_LOADER` | `OFF` | 任意の PNG ローダーを明示要求。 |
| `GYO_BUILD_SDL_TTF_ADAPTER` | `OFF` | 任意の SDL_ttf テキストラスタライザーを明示要求。 |
| `GYO_BUILD_UFBX_LOADER` | `OFF` | 任意の ufbx モデルローダーを明示要求。中立 Model／Collision は独立。 |
| `BUILD_TESTING` | `ON` | doctest と結合テストを CTest に登録。 |

App の要求と明示的な adapter 選択を統合しますが、ユーザーの cache オプションは書き換えません。Engine、Input、Model、Collision、Text、Render、UI は具体的なゲームから独立します。App、tool、任意 adapter を無効にすれば SDL、SDL_image、SDL_ttf、ImGui を必要としません。

### 描画とネイティブ開発

GPU app は共通 HLSL をオフラインで変換し、Windows は DXIL と SPIR-V、Linux は SPIR-V、macOS は Metallib を使います。ランタイムに HLSL コンパイラーはありません。Host ツールはビルドツリー内で別途構築し、macOS は選択 Xcode の Metal tools も必要です。`dev` のテストは CPU／headless と shader を対象にし、GPU テストは利用可能な display と GPU のある環境で別途実行します。`ci-windows`、`ci-linux`、`ci-macos` は CI 用のネイティブツールチェーン入口です。

CLion は既存の MSVC profile のまま CMake を再読込し、対象 app を選べます。Shader host の子ビルドは同じコンパイラーと Ninja を使います。任意の再配布ファイルがなくても、開発用構成・ビルドは可能です。macOS は `project()` 前に deployment target 13.3 を設定し、app と Metallib で共通にします。最低 OS 版は SDK の API 実装を保証しません。Xcode 16.4 に浮動小数点 `std::from_chars` がないため、ゲームの CSV 数値は十進構文と classic C++ locale、float 範囲検査で解析します。

Ubuntu は SDL の XTest 用に `libxtst-dev` が必要です。オフライン host は SDL video・dialog を無効にして `SDL_UNIX_CONSOLE_BUILD=ON` とし、macOS の未解決 Cocoa symbol と Linux の意図的な no-video 構成の拒否を防ぎます。これらは app 用 SDL の video 設定を変更しません。

### CI、配布、Release

[GitHub Actions](.github/workflows/cross-platform.yml) は Windows x64／MSVC、Linux x64／GCC 14、macOS ARM64／Xcode 16.4 で engine と UI editor を常にビルド・テストします。別に、固定 source commit の CSV から app × platform matrix を作ります。組み合わせごとに独立したビルドツリーと install stage を用い、対象 app と必要依存だけを含めます。Editor は app package に入りません。GPU／shader 不要の app はその処理を実行しません。

有効な app は配布版の startup テストを登録します。共通 runner は生成した `share/gyo/apps/<app>/manifest.json` を使い、package 外の作業ディレクトリーから実行します。App 固有の検証は quick／release、platform、GPU 条件で選択します。失敗、タイムアウト、必須証拠の欠落は package 化を阻止します。Object_FPS の startup、gameplay、欠落、Linux Lavapipe 検査は[app 検証ガイド](apps/object_fps/docs/acceptance.ja.md)を参照してください。App のない platform も engine／tool をテストします。全 app 無効は通常 CI で有効ですが、Prepare Release は事前に空集合を拒否します。

各組み合わせは `gyo-<name>-<platform>.tar.gz` と `.tar.gz.sha256` を生成し、archive 内には単一の `gyo-<name>` ルートを持ちます。Release は同じ commit の CSV から期待集合を計算し、app、platform、source SHA、release profile、必須ファイル、完全な証拠、checksum を検査します。Quick 証拠を release 証拠として使えません。Archive の安全性、相対 library path、依存検査も必須です。

**Actions → Prepare Release → Run workflow** で source branch、`v1.0.1` などの版番号、必要なら prerelease を指定します。固定 SHA で完全な profile を実行し、全必須検査成功後、書込権限を持つ job が tag と Draft Release と期待 package 集合を作ります。Actions Summary の Draft リンク、説明、添付を確認して **Publish release** を押します。公開や tag push は再ビルドを起動しません。CSV の版番号は個人の記録であり、この Release の版番号を供給しません。

通常の push／PR／手動 quick は Actions artifacts のみを生成します。同じ版の Draft 再試行は検証済み添付と手書きの説明を保持し、既存 tag に完全な SHA 一致を求め、公開済み版の置換を拒否します。同じ SHA を使う場合は元の実行を再実行し、新規 dispatch が新しい commit を選ぶ可能性に注意してください。手動入口を表示するには workflow を既定 branch へマージします。古い実行は元の定義を保持します。GUI と復旧の詳細：[日本語](docs/releasing.ja.md)・[繁體中文](docs/releasing.zh-Hant.md)。

App の内容は実行ファイル相対で配置し、非システム library は Windows の `bin`、Linux／macOS の `lib` に置きます。Windows Release／RelWithDebInfo は `cmake/GyoMsvcRuntime.cmake` が対応 MSVC runtime DLL を検出し、app と同梱します。検出できない場合は `GYO_MSVC_REDIST_DIR` を指定してください。開発ビルドは可能ですが、DLL 不足の release install は失敗します。Debug CRT は配布せず Windows 10+ の UCRT を使います。利用者に compiler や SDK は不要です。CI は VC import と Unix link を検査し、開発機の library が不完全な package を隠すことを防ぎます。

### 以前の検証結果

描画ガイドと日付付き開発ログの件数は、記載された commit・構成の履歴です。今回のプロジェクト管理変更や新しい matrix の hosted CI 合格を意味しません。[描画の検証状況](docs/rendering_architecture.ja.md#r10)には Windows CPU／GPU／配布、過去の Linux Lavapipe quick、macOS Metallib／CPU／配布、失敗した hosted run、未確認の物理 GPU の結果を残しています。旧 Object_FPS の GPU 無効構成も履歴です。現在は GPU 必須宣言により、この app の `GYO_RENDER_DEVICE=NONE` を拒否します。新しいビルドはそのログと Actions Summary で確認してください。

## 責務の概要

```text
Infrastructure は機能を提供する。
GYO は再利用可能なゲーム機構を提供する。
Game は方針とコンテンツを提供する。
```

現在の具体例：

- SDL が key、button、pointer、focus を通知し、`SdlInput` が GYO の物理入力フレームへ変換し、`InputActionMap` が名前付き action／axis を生成します。移動、照準、射撃、リロード、一時停止、メニュー操作の意味は Object_FPS が決めます。
- `NativeFileAssetSource` はカタログ解決後の実行時ファイルを読み、任意の SDL_image loader は CPU の `TextureAsset` にデコードします。`AssetManager` が識別子、handle、cache／寿命、loader dispatch を所有し、GPU texture は選択 render device だけが作ります。
- `FontLoader` は中立な `FontAsset` にフォントバイト列を保持します。`ITextRasterizer` は借用したフォント span と1つの UTF-8 文字列を、所有権を持つ CPU RGBA8 `TextBitmap` に変換します。任意の SDL_ttf adapter は `TTF_Font`、`SDL_Surface`、`SDL_Texture` を公開せず実装し、render device がアップロードして既存 sprite 経路で描画します。
- Object_FPS は不変な game snapshot を GYO `RenderQueue` に投影します。`Renderer` は camera／matrix と world／viewmodel／post／HUD pass を準備し、`IRenderDevice` は不透明 GPU handle と準備済み frame の実行を担当します。`ShaderLibrary` は不変 CPU shader artifact を device の資源から独立に保持します。SDL_GPU、D3D12、Vulkan、Metal の native handle は adapter 内だけにあります。

`GYO::Ui` は意図的に閉じた v1 標準です。JSON codec／validation、RectTransform、型付き binding／action、focus／hit test、button、slider、固定 step の list、順序付き draw list を提供します。`GYO::UiRenderer` は font／texture asset を解決し、上限のある文字列全体の cache を管理して Overlay sprite だけを送ります。Rich text、shaping、localization、Flex／Grid、script、widget／plugin ABI は対象外です。[docs/ui_toolchain.md](docs/ui_toolchain.md)を参照してください。

## Object_FPS は適合性を検証する利用側

この統合は KamataEngine のソース同士を置き換える移植ではありません。依存方向は次のとおりです。

```text
Object_FPS のゲーム方針・コンテンツ
        |
        v
GYO lifecycle + input actions + assets + render contracts + runtime port
        |
        v
選択した SDL platform / SDL_GPU / SDL_image
```

旧ゲームが再利用可能な必要性を示した場合は、中立な機構を先に GYO へ追加してから Object_FPS が利用します。別の入力 framework、resource manager、renderer contract、application loop は持たず、ゲーム固有名や状態は GYO Core の外に置きます。

現在の map と CSV は適合性検証データであり、engine の固定知識ではありません。Asset ID は `object_fps.*` の名前空間に登録し、使うコンテンツは campaign data が選びます。

### MVP の画面遷移

以下のコンテンツ、レイアウト、binding 値、action の結果は Object_FPS が所有します。

- MainMenu：Start Game、Controls、Quit。
- Controls：入力操作の説明と Back。
- Pause：ゲーム表示の上に Gamma、Exposure、Resume、Main Menu、Quit。
- Results：campaign の結果、room の結果、Main Menu への復帰。
- Playing HUD：crosshair、HP、弾倉／予備弾、reload 状態、現在 stage の情報。

### ジャンプと Mark-23

- **Space** は接地中のジャンプ1回を実行します。足位置、hit capsule、camera、射撃原点が一緒に移動し、押し続けても反復しません。高さ 0.6 m、重力 18 m/s² が既定値で、壁・敵の grid blocking は空中でも有効です。
- **左クリック**でセミオート Mark-23 を射撃し、**R** で reload、**H** で収納／取り出しを行います。新 campaign は Draw から始まり、native animation 時間は Shoot 0.333 s、Reload 3.733 s、Draw 0.833 s、Hide 0.367 s。初期弾数は弾倉／予備 12/48、弾の移動は reload 完了時だけです。
- モデルはアニメーション付きの手、slide、magazine と3つの diffuse texture を含みます。右下の独立 camera が自己遮蔽を保ち、近くの壁は武器を切り取りません。Scene の exposure／gamma を適用します。
- `assets/object_fps/data/mark23_viewmodel.json` は model／material／clip ID、固定 Idle anchor、repeat sampler、camera 相対 offset、rotation、scale、武器 FOV を定義します。調整済み offset は `(0.12, -0.18, 0.55)` m、垂直 FOV は 55°。銃口は `main_j` の local 座標で定義します。共通 loader が Shoot の時刻0と配置から武器ごとの射撃幾何を求め、武器 55°／world 60° の FOV 変換が画面上の位置を保ちます。配置と銃口の再調整は編集・asset staging のビルド・再起動で反映し、CSV action 時間は変えません。
- 命中判定は新しい反動前の照準を使用します。見た目の tracer は既存 projectile 更新後の反動付き描画フレームで銃口から発生し、world 遮蔽を適用しますが二重にダメージを与えません。過去の GPU 投影検査は3つの aspect ratio で合格しました。[調整記録](docs/dev_logs/2026_09_15_model_muzzle_calibration.md)を参照してください。
- Door は独立カタログを通じて実際の `assets/common/white1x1.png` を使います。両 asset root を配布し、catalog path がルートを脱出することを認めません。

モデル分析、所有境界、検証、制限は[反復開発記録](docs/dev_logs/2026_09_15_mark23_jump.md)を参照してください。

非 Playing の4画面は `assets/object_fps/ui/screens.json` から一度だけ読みます。コンパイル済み fallback、live link、hot reload はありません。`UiRuntime` が selection、focus、pointer capture、hit testing を担当し、Object_FPS が不透明 action ID を型付き command に変換します。Playing HUD の方針は C++ のまま同じ `UiDrawList` を出力します。GYO は Object_FPS の menu action の意味を知りません。

実行ファイルの smoke 経路は異なる境界を検査します。

- `--startup-smoke-test` は window／GPU を作らず配布内容を読み込みます。Quick と release は3プラットフォームの対象 package を別の作業ディレクトリーから起動します。
- `--headless-smoke-test` は window／GPU なしでゲーム開始、jump、pause／resume、射撃、reload 時間も検査します。Release はこの重いケースを実行します。
- `object_fps.smoke` は Playing に入り、world frame の送信・表示を確認します。
- `object_fps.menu_smoke` は通常 MainMenu を起動し、最初の menu frame に可視 UI submission がない場合に失敗します。
- `object_fps.viewmodel_smoke` は Mark-23 の各 action の開始・中央・終端を表示します。`--viewmodel-smoke-test --capture-dir <directory>` で診断 scene frame を保存し、`--preview-4x3`／`--preview-21x9` も指定できます。
- `object_fps.reload_smoke` は元 frame 間の短い手首動作を含む Reload 全体を 60 Hz で送信します。`--reload-smoke-test --capture-dir <directory>` で全画像列を保存します。[Reload と銃口の修正](docs/dev_logs/2026_09_15_reload_muzzle_fix.md)を参照してください。
- `object_fps.muzzle_smoke`、`object_fps.muzzle_smoke_4x3`、`object_fps.muzzle_smoke_21x9` は7つの camera 条件で GPU 銃口 marker を比較します。16:9 は `--muzzle-smoke-test --capture-dir <directory>`、他の比率は `--preview-4x3`／`--preview-21x9` を加えます。Model review 画像の cyan は調整済み銃口です。[調整記録](docs/dev_logs/2026_09_15_model_muzzle_calibration.md)にコマンド、誤差、証拠があります。

過去の銃口調整では、再ビルドした headless の 1,169 assertions を含む18件の結合検査を完了しました。3つの GPU aspect ratio 検査では marker 間の誤差は 0.0000 pixel、理論投影との差は最大 0.3823 pixel でした。

Headless の `ObjectFpsUi` テストは実際の JSON を解析し、日本語 label、4 canvas、Results binding、C++ HUD、JSON 並び替え後の action ID、同一 frame の slider 更新を SDL_ttf／render backend なしで検証します。

## Runtime Boundary と Weaver

呼出元に依存しない境界です。

```text
Game / Test / Debug Console / Replay / AI / Weaver
                         |
                         v
               GYO Runtime Boundary
              Query / Command / Event
                         |
                         v
                 GYO の機構
```

`IRuntimePort<Snapshot, Command, Event>` が現在の最小の型付き境界です。現在の不変 snapshot を query し、合法な型付き intent を submit し、runtime が出した型付き事実を参照できます。`Query()` と `Events()` は次の runtime 更新まで有効な借用 view なので、保持する内容は caller がコピーします。Payload の意味は個別 runtime／game domain に属し、interface の形、所有規則、backend pointer を渡さないことが GYO の標準です。

完全な world／entity API、remote protocol、command scheduler、汎用 event bus ではありません。現在の利用側が責務を明らかにするまで、それらは保留します。

Weaver は未実装です。任意の外部高階 runtime であり、将来も game、test、debug tool と同じ公開機構を使います。Weaver なしで普通のゲームをビルド・実行できる必要があり、GYO Core が Weaver に依存することはありません。

## 成長の規則

機能は責務のある場所へ追加します。Text backend、Render3D、Physics3D、Navigation、外部 controller の統合は、通常は固有の module／backend／adapter の追加で行い、無関係な Asset、Input、Render、Runtime、game を書き換えません。

Editor ecosystem、node tree、汎用 ECS、visual scripting、plugin framework、完全な RenderGraph／physics engine、大規模 DI container、generic manager、空 interface を先に作りません。現在のコードに具体的な責務と実際の caller があるときに抽象化を導入します。

## 命名

| 種類 | 規則 | 例 |
|---|---|---|
| ディレクトリー | 小文字、必要なら `_` で区切る | `render/backend/sdl_gpu` |
| C++ namespace | PascalCase または engine をルートにする | `Engine::Asset` |
| C++ source／header | PascalCase | `RuntimeLoop.hpp` |
| ゲーム別 asset root | 小文字のゲーム識別子 | `assets/object_fps/` |
| データファイル | 小文字 | `asset_catalog.json`、`levels.csv` |
