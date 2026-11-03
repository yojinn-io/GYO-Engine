# PvP v8：pv9-rename（proto 與套件改名為 v9）

日期：2026-10-31。分支 `claude/pvp-v8-after-pv8`。計畫的檔位是 low｜Sonnet 5.5；照 pv8-rename 的先例由主會話（Opus 5.5）直接做：只有機械式的替換，交給子 agent 還要另外說明 prompt。估時 1～2 h，實際約 40 分鐘。

**狀態：** 完成。版本值仍是 8；改成 9 在 hitbox-line 的 wire 變更時做（[protocol-v9](../object_fps_pvp/protocol-v9.zh-Hant.md) §1，`pv9-final-approve`）。

## 做了什麼

- `client_v8.proto`／`runtime_v8.proto` 改名為 `client_v9.proto`／`runtime_v9.proto`；package 改為 `object_fps_pvp.client.v9`／`object_fps_pvp.runtime.v9`，`go_package` 改為 `clientv9`／`runtimev9`，生成的 Go 檔與目錄跟著改。
- Go 的 import、C++ 的 `namespace pb`（`IpcHost.cpp`、`ClientConnection.cpp`）與 include、CMake target、驗收工具（`network_main.cpp`、`worker_main.cpp`）、`IpcHostTests.cpp`、`test_drop_reasons.py`。
- `protocol/README.md` 加註「檔名先改成 v9，版本值仍是 8」；[network-architecture](../object_fps_pvp/network-architecture.zh-Hant.md) 裡的檔名。
- 不改：版本值、欄位、protocol-v8 條文裡的檔名、歷史紀錄（dev_log、計畫文件）。

## 檢驗

- 重產：專案的 protoc 36.2（`_deps/protobuf-build/protoc`）與 protoc-gen-go v1.36.11。先從 `a5bc7778` 的 proto 重產 v8，和已提交的檔案位元一致；再重產 v9，和「舊檔把 `client_v8`、`clientv8`、`client.v8`、`v8.proto`（runtime 同）換成 v9」差異 0 行。
- Go：`go vet ./...`、`go test ./...` 通過。
- C++：刪掉建置樹裡殘留的 `*_v8.pb.*` 後重新設定、全部建置；CTest 104／104 通過。
- 權威 digest 的兩樹比對：base `a5bc7778`（改名前，暫存區的 worktree）、branch＝本批，同一台 Intel Mac，`compare_authority_trees.py` 各 1 次：scale 1、scale 3 都是「42 scenarios, 0 differing」。
- 突變（D184）：`mutations.json` 沒有任何 find 落在改名的識別字上，相關突變 0 個，沒有跑。
