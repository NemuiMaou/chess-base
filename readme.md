Fork or clone your this chess project into a new GitHub repository.

Add support for FEN stringsLinks to an external site. to your game setup so that instead of the current way you are setting up your game board you are setting it up with a call similar to the following call.

FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

Your routine should be able to take just the board position portion of a FEN string, or the entire FEN string like so:

FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

(you can ignore the end for now)

This will allow you to quickly check that your castling, promotion and en passant code is working.

(update 1)
implemented movement for knights, kings, and pawns. I feel like I didnt really make any crazy changes, using the classes and videos as inspiration I was able to figure out how to easily get the knight and king movement working. The pawns were a little tricky though, I was having trouble with my engine detecting which side the pawns were actually on so I added some extra helper functions to sort that out. I think the biggest thing I will currently need to fix is the assignment of all my pieces since I currently have it assigned to where white is player 1 and black is player 0. I haven't touched that yet and was able to still get the turns and moves for the pieces working so, maybe I don't need to change it.
