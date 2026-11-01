# スカラーspecifyの期待値移行

[English](specify-expectation-migration.md)。2026-11-02。

ネイティブ実行を示した後、この移行を保守者が別途承認した。
[承認済みスカラー契約](specify-p1-proposal.ja.md)の1〜8節が対象のmodule pathを定める。
IEEE 1800の参照先は30章（特に30.5〜30.7）であり、契約に記した版の確認の制限は残す。

`tests/cli/unsupported_matrix/specify_path.sv`のバイト列は変えない。
SHA-256は`3f827e060bc17d4aa07ef1ce4876afccaeb4b601ad0de294aa87cb10f1b792f3`。
分類を`unsupported_matrix.accepted`へ移し、`unsupported_matrix.stderr`から、この入力の
古い拒否期待行だけを削除する。生成器の`ACCEPTED`集合を更新し、入力とソース生成規則が
変わらないので生成版19は維持する。CLIランナーは終了コード2に代えて、終了コード0と
空の標準出力を要求する。ほかの拒否検査は維持する。この入力のpathはtick 1で落ち着く。
XFAIL、検査の無視、未実行のPASSへの変更ではない。

タイミングの意味は、新規121件のネイティブ行列と共有カーネル・SIR・観測のテストで
別に検証する。凍結したP1プローブの早い時点の0と遅い時点の1も含む。
この分類・期待値の移行は`src/`と別コミットにする。
