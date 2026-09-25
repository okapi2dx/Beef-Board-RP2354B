# Beef Board RP2354B

Beef Board V2 の RP2354B 版です。RP2354B 基板、KiCad 回路図、部品表、Pico SDK 製ファームウェアを収録しています。STM32 のファームウェアや書き込み手順は使用しません。

## 内容

| 場所 | 内容 |
|---|---|
| `hardware/Beef-Board-RP2354B.kicad_pro` | KiCad 10 プロジェクト |
| `hardware/Beef-Board-RP2354B.kicad_pcb` | 完成版として指定された基板 |
| `hardware/Beef-Board-RP2354B.kicad_sch` | 基板に合わせた回路図 |
| `hardware/BEEF-BOARD.pretty/` | プロジェクト用フットプリント |
| `hardware/BOM-budget.csv` | 暫定部品表と概算価格 |
| `firmware/Beef-Board-RP2354B.uf2` | BOOTSEL 書き込み用ファームウェア |
| `firmware/src/` | ファームウェアのソースコード |
| `firmware/configurator-overrides/` | 既存 Beef Board Configurator に適用する RP2354B 対応ファイル |

基板ファイルは元の完成版を**内容変更なし**で複製し、ファイル名のみこのリポジトリ用に変更しました。元ファイルと複製後の SHA-256 はどちらも `809799BD11535BDA2E2E1D615DC2C81EE66FC8C3E554AD3BAF7050547B10E662` です。

## 状態

- 基板は設計上の完成版として扱います。KiCad DRC の未配線は 0 件です。
- 回路図と基板の比較には USB-C 複合パッドの表現差などを含む指摘が残っています。実機評価および製造業者による確認は未実施です。
- BOM の価格・候補型番は暫定です。特に J4 の USB-C コネクタ、J3 端子台、一部コンデンサの仕様を発注前に確認してください。
- ファームウェアは RP2354B 向けにビルド済みですが、実機での USB・入出力・書き込み動作は未検証です。詳細は [firmware/README.md](firmware/README.md) を参照してください。

## 書き込み

J60 を短絡して J61 でリセットするか、接続済みファームウェアから BOOTSEL コマンドを送ります。J1 の USB 接続で ROM の USB ドライブが表示されたら `firmware/Beef-Board-RP2354B.uf2` をコピーしてください。J60 の短絡は USB ドライブが現れたら外します。

Configurator の対応ファイルは差分一式で、単体の Configurator アプリではありません。既存の Beef Board V2 Configurator に適用してください。RP2354B 用のファームウェア選択と BOOTSEL 案内を追加し、STM32 用の書き込み経路は表示しません。
