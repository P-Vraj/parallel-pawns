# Opening Books

This directory holds the opening books used for engine matches and benchmarking, typically through [Fastchess](https://github.com/Disservin/fastchess).

Book files are large `.epd` or `.pgn` files and are not tracked by version control for this project. A good source for downloading these books is from the [Stockfish](https://github.com/official-stockfish/books) project.

Currently:
* `scripts/run-fastchess.sh` defaults to `data/openings/UHO_Lichess_4852_v1.epd` (download ZIP [here](https://github.com/official-stockfish/books/blob/master/UHO_Lichess_4852_v1.epd.zip)).
* Override the path with `BOOK_PATH=/path/to/book.epd ./scripts/run-fastchess.sh`

*Note*: Prefer `.epd` books for fast engine-vs-engine runs when available.