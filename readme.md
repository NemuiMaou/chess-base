Fork or clone your this chess project into a new GitHub repository.

Add support for FEN stringsLinks to an external site. to your game setup so that instead of the current way you are setting up your game board you are setting it up with a call similar to the following call.

FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");

Your routine should be able to take just the board position portion of a FEN string, or the entire FEN string like so:

FENtoBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

(you can ignore the end for now)

This will allow you to quickly check that your castling, promotion and en passant code is working.

(update 1)
implemented movement for knights, kings, and pawns. I feel like I didnt really make any crazy changes, using the classes and videos as inspiration I was able to figure out how to easily get the knight and king movement working. The pawns were a little tricky though, I was having trouble with my engine detecting which side the pawns were actually on so I added some extra helper functions to sort that out. I think the biggest thing I will currently need to fix is the assignment of all my pieces since I currently have it assigned to where white is player 1 and black is player 0. I haven't touched that yet and was able to still get the turns and moves for the pieces working so, maybe I don't need to change it.

(update 2)
for this update, i finished the implementation, using the document provided as a guide, for the negamax ai in my chess engine. I also finished implementing legal moves for the rest of my pieces. That was a little harder since i tried using the same method for the knights, kings, and pawns but it didnt work out very well, so i used a helper method that just makes a mask for the queen, rook, and bishop to see where they can "slide" to. For the ai, i only just finished getting it to work and play whilst following and working from the documentation provided. the current depth i can reach at the moment is 3 and the ai does play and will take but i wouldnt say its very good at protecting or defending. I would still need to work and add more logic for actually winning/losing and other rules that i havent attempted yet (like castling, en passent, small things like that). to reach a further depth i think i would need to channge the way i wrote my generate all moves because i just mainly used what i did for the legal moves of a human player.
