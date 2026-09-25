# Beef Board RP2354B

Beef Board V2 の RP2354B 版です。RP2354B 基板、KiCad 回路図、部品表、Pico SDK 製ファームウェアを収録しています。STM32 のファームウェアや書き込み手順は使用しません。

## 内容

| 場所 | 内容 |
|---|---|
| `hardware/Beef-Board-RP2354B.kicad_pro` | KiCad 10 プロジェクト |
| `hardware/Beef-Board-RP2354B.kicad_pcb` | 指定された基板の配線整理版 |
| `hardware/Beef-Board-RP2354B.kicad_sch` | 基板に合わせた回路図 |
| `hardware/BEEF-BOARD.pretty/` | プロジェクト用フットプリント |
| `hardware/BOM-budget.csv` | 暫定部品表と概算価格 |
| `hardware/ROUTING.md` | 配線整理の変更量と残る確認事項 |
| `hardware/DRC-efficient.json` | 配線整理後の KiCad DRC 結果 |
| `hardware/routing-changes.json` | 採用した配線変更の記録 |
| `firmware/Beef-Board-RP2354B.uf2` | BOOTSEL 書き込み用ファームウェア |
| `firmware/src/` | ファームウェアのソースコード |
| `configurator/` | RP2354B 対応 Configurator のソース一式（Web UI・Windows デスクトップ版） |
| `configurator/Beef-Board-RP2354B-Configurator-V1.00.exe` | Windows 64-bit 用ポータブル版 |

現在の PCB は今回指定されたファイルを基準に配線整理した版です。部品位置とビアは入力時のままです。変更量は [`hardware/ROUTING.md`](hardware/ROUTING.md) を参照してください。

## 状態

- 基板は設計上の完成版として扱います。KiCad DRC の未配線は 0 件です。
- 回路図と基板の比較には USB-C 複合パッドの表現差などを含む指摘が残っています。実機評価および製造業者による確認は未実施です。
- BOM の価格・候補型番は暫定です。特に J4 の USB-C コネクタ、J3 端子台、一部コンデンサの仕様を発注前に確認してください。
- ファームウェアは RP2354B 向けにビルド済みですが、実機での USB・入出力・書き込み動作は未検証です。詳細は [firmware/README.md](firmware/README.md) を参照してください。

## 電源選択（J2）

スライドスイッチの代わりに、2.54 mm ピッチの3ピンヘッダ J2 とショートピンを使います。J2 の1–2番を短絡すると J1 のみで給電し、2–3番を短絡すると J1 は本体・ボタンLEDへ給電し、J4 はショットキーダイオード D1 経由で VAUX 系負荷へ給電します。使用時は隣接する一組だけを短絡してください。J2 の電流定格は接続する負荷に合わせて選定してください。
## 書き込み

J60 を短絡して J61 でリセットするか、接続済みファームウェアから BOOTSEL コマンドを送ります。J1 の USB 接続で ROM の USB ドライブが表示されたら `firmware/Beef-Board-RP2354B.uf2` をコピーしてください。J60 の短絡は USB ドライブが現れたら外します。

Configurator は同梱のポータブル版を実行するか、[`configurator/README.md`](configurator/README.md) に従ってソースからビルドできます。RP2354B 用のファームウェア選択と BOOTSEL 案内があり、旧 AT90USB1286 も引き続き扱えます。STM32 用の書き込み経路は含みません。
