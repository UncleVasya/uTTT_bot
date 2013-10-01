#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <ctime>
#include <climits>
using namespace std;

// TODO: make a GameState object that holds info about
//		 current player, active subboard, previous moves, etc.

// TODO: keep an updating list of possible moves on every subboard;
//		 in moveGenerator gather this info; 
//		 this should be faster than generating from scratch.

// TODO: make use of Board.parent (for example, in unmakeLastMove);

// TODO: make arrays of boards of specific level;
//		 array with level=LEVELS will replace Grid object.

// TODO: rewrite makeMove() and unmakeMove() from recursion into loop

// TODO: improve minimax_status. make it keep:
//		 - current & best move & score for every ply;
//		 - total processed positions;
//		 - approximation of positions left;
//		 - approximation of truncated by alpha-beta positions;
//		 - ...

// TODO: improve printMinimaxStatus():
//		 - let it print some first, middle and last plyes;


// TODO: clare Move type meaning. In some places it used as an absolute coordinate
//		 while in another places as a relative one.
//		 One way is to replace Move with Coord.

// TODO: make use of getPossibleLines() in isWinner() function;

// TODO: think about making Line consist of Board& objects instead of coords;

// FIXME: there can be a situation when subboard will have 0 possible lines
//		  but its value is empty yet. In this case upper board will think
//		  that line with this subboard is possible. But it's not.

// TODO: replace Pair with Struct in Move type;

// TODO: cache board scores so we don't need to recalculate them
//		 when processing different lines;

// TODO: change state.current_player in makeMove() and unmakeMove();

const int BOARD_HEIGHT 		= 3;
const int BOARD_WIDTH  		= 3;
const int BOARD_SIZE   		= BOARD_HEIGHT * BOARD_WIDTH;

const int LEVELS       		= 2;
const int WIN_LINE_LENGTH 	= 3;

const char* SAVE_FILENAME 	= "game.sav";

#define UNDEFINED -1

typedef enum {Empty, Cross, Nough, Draw}    BoardValue;
typedef struct Board Board;
struct Board{
	int         row, col; // mapped to global grid
	int         level;
    BoardValue  value;
	int 		empty_cells;
	int			changed_at;
    Board*      subboards[BOARD_HEIGHT][BOARD_WIDTH];
	Board*		upboard;
};

typedef pair <int,int> 			Move;
typedef vector <Move> 			Moves;

typedef struct State State;
struct State{
	Board 		board;
	Moves 		moves;
	int 		move_num;
	BoardValue	current_player;
};

static const int GRID_HEIGHT = pow(BOARD_HEIGHT, LEVELS);
static const int GRID_WIDTH  = pow(BOARD_WIDTH,  LEVELS);
typedef vector<vector<BoardValue> > Grid;

typedef struct PlyStatus PlyStatus;
struct PlyStatus{
	BoardValue player;
	int moves_done;
	int moves_total;
	Move best_move;
	Move current_move;
	int best_score;
	int current_score;
};

typedef struct MinimaxStatus MinimaxStatus;
struct MinimaxStatus{
	long positions_done;
	vector<PlyStatus> plyes;
};
MinimaxStatus minimax_status;

bool USE_EVAL_POS = true;

//#define LOG_EVAL_POS
//#define LOG_MINIMAX

BoardValue charToValue(char ch){
	switch (ch){
		case '-': return Empty;
		case 'X': return Cross;
		case 'O': return Nough;
	}
}

char valueToChar(BoardValue val){
	switch (val){
		case Empty: return '-';
		case Cross: return 'X';
		case Nough: return 'O';
		case Draw:	return 'D';
	}
}

Board& getEmptyBoard(){
	Board* board 		= new Board;
	board->row 			= 0;
	board->col 			= 0;
	board->value 		= Empty;
	board->empty_cells 	= BOARD_SIZE;
	board->changed_at	= UNDEFINED; 	// wasn't changed yet
	board->level 		= 0;
	
	return *board;
}

Move getEmptyMove(){
	Move move;
	move.first 	= UNDEFINED;
	move.second = UNDEFINED;
	return move;
}

void initSubboards(Board& board, int level = 1){
	for	(int row=0; row<BOARD_HEIGHT; ++row){
		for (int col=0; col<BOARD_WIDTH; ++col){
			Board& brd 	= getEmptyBoard();
			brd.row 	= board.row * BOARD_HEIGHT + row;
			brd.col 	= board.col * BOARD_WIDTH  + col;
			brd.level 	= level;
			
			board.subboards[row][col] = &brd;
			if (level < LEVELS) 
				initSubboards(brd, level+1);
		}
	}
}

void addCellToLine(Moves& line, int row, int col){
	Move cell;
	cell.first 	= row;
	cell.second = col;
	line.push_back(cell);
}

// TODO: make diagonals code more general;
// 		 now it works only with 
//		 main diagonals on square grid
vector<Moves>& getAllLines(Board& board){
	static vector<Moves> lines;
	
	if (lines.size() == 0){
		Moves line;

		// horisontal lines
		for (int row = 0; row<BOARD_HEIGHT; ++row){
			line.clear();
			for (int col=0; col<BOARD_WIDTH; ++col){
				addCellToLine(line, row, col);
			}
			lines.push_back(line);
		}
		
		// vertical lines
		for (int col = 0; col<BOARD_WIDTH; ++col){
			line.clear();
			for (int row=0; row<BOARD_WIDTH; ++row){
				addCellToLine(line, row, col);
			}
			lines.push_back(line);
		}
		
		// left-top diagonal
		line.clear();
		for (int row=0; row<BOARD_HEIGHT; ++row){
			addCellToLine(line, row, row);
		}
		lines.push_back(line);
		
		// right-top diagonal
		line.clear();
		for (int row=0; row<BOARD_HEIGHT; ++row){
			addCellToLine(line, row, BOARD_HEIGHT-1-row);
		}
		lines.push_back(line);
	}
	return lines;
}


bool isWinner(Board& board, BoardValue player){
	vector<Moves>& lines = getAllLines(board);
	int len = lines.size();
	for (int i=0; i<len; ++i){
		Moves& line = lines[i];
		bool is_complete = true;
		int len = line.size();
		for (int i=0; i<len; ++i){
			Move& cell = line[i];
			BoardValue cell_value = board.subboards[cell.first][cell.second]->value;
			if (cell_value != player) 
				is_complete = false;
		}
		if (is_complete) 
			return true;
	}
	return false;
}

bool isBoardFull(Board& board){
	return (board.empty_cells == 0);
}

bool isBoardEmpty(Board& board){
	return (board.empty_cells == BOARD_SIZE);
}

BoardValue boardValue(Board& board){
	if 		(board.value != Empty) 		return board.value; // value determined already
	else if (isWinner(board, Cross)) 	return Cross;
	else if (isWinner(board, Nough))	return Nough;
	else if (isBoardFull(board)) 		return Draw; 		// board is full
	else								return Empty;
}

BoardValue nextPlayer(BoardValue player){
	switch (player){
		case Cross: return Nough;
		case Nough: return Cross;
	}
}

void boardToGrid(Board& board, Grid& grid, int level = 0){
	if (level < LEVELS){
		for (int row=0; row<BOARD_HEIGHT; ++row)
			for (int col=0; col<BOARD_WIDTH; ++col)
				boardToGrid(*board.subboards[row][col], grid, level+1);
	}
	else{
		grid[board.row][board.col] = board.value;
	}
}

void printMoves(Moves moves, ostream& stream = cout){
	for (Moves::iterator move = moves.begin(); move != moves.end(); ++move){
		stream << "(" << move->first << "," << move->second << ")";
	}
}

void saveMoves(Moves moves, ofstream& save_file){
	save_file << moves.size() << endl;
	for (Moves::iterator move = moves.begin(); move != moves.end(); ++move){
		save_file << move->first << " " << move->second << endl;
	}
}

Moves loadMoves(ifstream& save_file){
	Moves moves;
	size_t moves_num;
	save_file >> moves_num;
	for (size_t i=0; i<moves_num; ++i){
		Move move;
		save_file >> move.first >> move.second;
		moves.push_back(move);
	}
	return moves;
}

// TODO: Generalize. Now it works only for LEVELS=2
Board& getSubboardForMove(Board& board, Move move){
	int row, col;
	if (board.level < LEVELS-1){
		row = move.first  / BOARD_HEIGHT;
		col = move.second /	BOARD_WIDTH;
	}
	else{
		row = move.first  % BOARD_HEIGHT;
		col = move.second %	BOARD_WIDTH;
	}
	return *board.subboards[row][col];
}

void makeMove(State& state, Board &board, Move move, BoardValue player){
	if (board.level < LEVELS){
		Board& subboard = getSubboardForMove(board, move);
		makeMove(state, subboard, move, player);
		
		// update board if subboard has changed
		if (subboard.changed_at == state.move_num){
			--board.empty_cells;
			BoardValue new_value = boardValue(board);
			if (board.value != new_value){
				board.value = new_value;
				board.changed_at = state.move_num;
			}
		}
		
		// update game state at the very end of makeMove
		if (board.level == 0){
			state.moves.push_back(move);
			++state.move_num;
		}
	}
	else{
		board.value 		= player;
		board.changed_at 	= state.move_num;
	}
}

void unmakeLastMove(State& state, Board &board){
	if (board.level < LEVELS){		
		// reset board changes if it was changed on last move
		if (board.changed_at == state.move_num-1){
			board.value = Empty;
			board.changed_at = -1;
		}
		Board& subboard = getSubboardForMove(board, state.moves.back());
		if (subboard.changed_at == state.move_num-1){
			++board.empty_cells;
		}
		unmakeLastMove(state, subboard);
	}
	else{
		board.value 		= Empty;
		board.changed_at 	= -1;
		state.moves.pop_back();
		--state.move_num;
	}
}

void addMovesToGrid(Moves moves, Grid &grid){
	BoardValue player = Cross;
	for (Moves::iterator move = moves.begin(); move != moves.end(); ++move){
		grid[move->first][move->second] = player;
		player = nextPlayer(player);
	}
}

// TODO: you can rewrite it with something like: 
//		 vector<BoardValue> grid_row(GRID_WIDTH, Empty);
Grid& getEmptyGrid(){
	Grid& grid = *(new Grid);
	for (int row=0; row<GRID_HEIGHT; ++row){
		vector<BoardValue> grid_row;
		for (int col=0; col<GRID_WIDTH; ++col){
			grid_row.push_back(Empty);
		}
		grid.push_back(grid_row);
	}
	return grid;
}

Grid& readGrid(istream& input = cin){
	Grid& grid = getEmptyGrid();
	string row_str;
    for(int row=0; row<GRID_HEIGHT; ++row){
		input >> row_str;
		for (int col=0; col<GRID_WIDTH; ++col){
			grid[row][col] = charToValue(row_str[col]);
		}
	}
	return grid;
}

void printGrid(Grid& grid){
	cout << " ";
	for (int col=0; col<GRID_HEIGHT; ++col){
		if (col % 3 == 0) cout << "  ";
		cout << col;
	}
	for(int row=0; row<GRID_HEIGHT; ++row){
		if (row % 3 == 0) cout << endl;
		cout << row;
		for (int col=0; col<GRID_WIDTH; ++col){
			if (col % 3 == 0) cout << "  ";
			cout << valueToChar(grid[row][col]);
		}
		cout << endl;
	}
}

void printBoard(Board& board){
	if (board.level < LEVELS){
		for (int row=0; row<BOARD_HEIGHT; ++row){
			for (int col=0; col<BOARD_WIDTH; ++col){
				cout << valueToChar(board.subboards[row][col]->value);
			}
			cout << endl;
		}
	}
	else if (board.level == LEVELS){
			cout << valueToChar(board.value) << endl;
	}
}

void printBoardByLevel(Board& board, int level){
	if (board.level == level){
		printBoard(board);
	}
	else{
		for (int row=0; row<BOARD_HEIGHT; ++row){
			for (int col=0; col<BOARD_WIDTH; ++col){
				cout << "Level: " << board.level+1 << "\t coords: " << row << " " << col << endl;
				printBoardByLevel(*board.subboards[row][col], level);
			}
		}
	}
}

bool isGameEnded(State& state){
	return (state.board.value != Empty);
}

void printGameState(State& state){
	cout << endl << endl;
	Grid grid = getEmptyGrid();
	boardToGrid(state.board, grid);
	printGrid(grid);
	
	cout << endl << endl;
	printBoard(state.board);

	cout << endl << endl;
	if (state.moves.size() > 0){
		cout << valueToChar(nextPlayer(state.current_player)) << " placed at  "
			 << state.moves.back().first << " " << state.moves.back().second;
	}
	cout << endl << endl;
	switch (state.board.value){
		case Empty:	
			cout << "move " << state.move_num << endl;
			cout << valueToChar(state.current_player) << " moves";
			break;
		case Cross: 
		case Nough:
			cout << valueToChar(state.board.value) << " wins in " << state.move_num << " moves";
			break;
		case Draw:	
			cout << "It's a Draw in " << state.move_num << " moves";
			break;
	}
	cout << endl << endl;
}

Board getNextBoardForMove(Board& board, Move move){
	int row = move.first  % BOARD_HEIGHT;
	int col = move.second % BOARD_WIDTH;
	return *board.subboards[row][col];
}

Board getNextBoard(State& state){
	return getNextBoardForMove(state.board, state.moves.back());
}

vector<Board>& getActiveBoards(State& state){
	vector<Board>* boards = new vector<Board>;
	
	bool move_anywhere = true;
	if (state.moves.size() > 0){
		Board next_board = getNextBoard(state);
		if (next_board.value == Empty){
			// player can move only on this board
			move_anywhere = false;
			boards->push_back(next_board);
		}
	}
	
	if (move_anywhere){
		// player can move on any non-full board
		for (int row=0; row<BOARD_HEIGHT; ++row){
			for (int col=0; col<BOARD_WIDTH; ++col){
				Board& brd = *state.board.subboards[row][col];
				if (!isBoardFull(brd)){
					boards->push_back(brd);
				}
			}
		}
	}
	return *boards;
}

Moves& getPossibleMoves(Board& board){
	Moves* moves = new Moves;
	for (int row=0; row<BOARD_HEIGHT; ++row){
		for (int col=0; col<BOARD_WIDTH; ++col){
			Board& brd = *board.subboards[row][col];
			if (brd.value == Empty){
				Move move;
				move.first 	= brd.row;
				move.second = brd.col;
				moves->push_back(move);
			}
		}
	}
	return *moves;
}

Moves& generateMoves(State& state){
	Moves* moves = new Moves;
	vector<Board>& active_boards = getActiveBoards(state);
	for (vector<Board>::iterator brd = active_boards.begin(); brd != active_boards.end(); ++brd){
		Moves& board_moves = getPossibleMoves(*brd);
		moves->insert(moves->end(), board_moves.begin(), board_moves.end());
		delete(&board_moves);
	}
	delete(&active_boards);
	return *moves;
}

Move getRandomMove(State& state){
	Moves& moves = generateMoves(state);
	int random = max(0,  rand() % (int)moves.size() - 1);
	Move move = moves[random];
	delete(&moves);
	return move;
}

bool isLinePossible(Board& board, Moves& line, BoardValue player){
	bool is_possible = true;
	for (int i=0; i<WIN_LINE_LENGTH; ++i){
		Move& cell = line[i];
		BoardValue cell_value = board.subboards[cell.first][cell.second]->value;
		if (cell_value != Empty && cell_value != player){
			is_possible = false;
			break;
		}
	}
	return is_possible;
}

void calcLinesCountByLength(Board& board, vector<Moves>& lines, BoardValue player, int count[]){
	// TODO: replace with memset()
	for (int i=0; i<WIN_LINE_LENGTH-1; ++i){
		count[i] = 0;
	}
	
	for (vector<Moves>::iterator line=lines.begin(); line != lines.end(); ++line){
		int length = 0;
		for (Moves::iterator cell = line->begin(); cell != line->end(); ++cell){
			if (board.subboards[cell->first][cell->second]->value == player){
				++length;
			}
		}
		++count[length];
	}
}

int calcBoardScore(Board& board, BoardValue player, int full_score);
int calcLineScore(Board& board, Moves& line, BoardValue player, int full_score);

int calcLineScore(Board& board, Moves& line, BoardValue player, int full_score){
	#ifdef LOG_EVAL_POS
		cout << endl << "calcLineScore():" << endl << endl;
		cout << "line: ";
		printMoves(line);
		cout << "  full_score: " << full_score << endl;
	#endif
	
	int line_score = 0;
	int len = line.size();
	for (int i=0; i<len; ++i){
		Move& cell = line[i];
		Board& cell_board = *board.subboards[cell.first][cell.second];
		int cell_score = calcBoardScore(cell_board, player, full_score/len);
		line_score += cell_score;
		
		#ifdef LOG_EVAL_POS
			cout << "cell: (" << cell->first << "," << cell->second << ")  score: " << cell_score << endl; 
			cout << "line_score: " << line_score << endl;
		#endif
	}
	#ifdef LOG_EVAL_POS
		cout << "=========================================================" << endl;
		cout << "=========================================================" << endl;
		cout << endl << endl;
	#endif
	
	return line_score;
}

int calcBoardScore(Board& board, BoardValue player, int full_score){
	int board_score;
	
	if (board.value == player){
		board_score = full_score; // board is ours!
	}
	else if (board.value != Empty){
		board_score = 0; // board is a Draw or belongs to enemy
	}
	else if (isBoardEmpty(board) && board.level >= LEVELS-1){
		// this is an empty board of lower level; 
		// there is nothing to calculate here;
		board_score = 0;
	}
	else{ // board is incomplete; calc its score
		#ifdef LOG_EVAL_POS
			cout << endl << "calcBoardScore():" << endl;
			cout << endl;
			printBoard(board);
			cout << endl;
			cout << "board value: " << valueToChar(board.value) << endl;
			cout << "player: " << valueToChar(player) << 
					"  full_score: " << full_score << endl << endl;
			cout << "board is incomplete. calc it:" << endl << endl;
		#endif
		
		
		vector<Moves>& lines = getAllLines(board);
		
		//int lines_count_by_length[WIN_LINE_LENGTH-1];
		//calcLinesCountByLength(board, possible_lines, player, lines_count_by_length);
		
		int best_line_score = 0;
		//int sum_line_score = 0;
		int len = lines.size();
		for (int i=0; i<len; ++i){
			Moves& line = lines[i];
			if (isLinePossible(board, line, player)){
				int line_score = calcLineScore(board, line, player, full_score);
				//sum_line_score += line_score;
				best_line_score = max(best_line_score, line_score);
			}
			
			#ifdef LOG_EVAL_POS
				cout << endl << "best line score: " << best_line_score << endl << endl;
			#endif
		}
	
		board_score = best_line_score;
		#ifdef LOG_EVAL_POS
			cout << "board_score: " << board_score << endl;
		#endif
		
		//board_score += sum_line_score / full_score; // bonus for good position
		
		board_score = board_score * board_score / full_score; // penalty for incompletness
		#ifdef LOG_EVAL_POS
			cout << "after PFI:   " << board_score << endl << endl;
		#endif
		
		#ifdef LOG_EVAL_POS
			cout << "=================" << endl;
			cout << "=================" << endl;
			cout << endl << endl;
			if (player == Nough){
				//string s; cin >> s;
			}
		#endif
	}
	
	return board_score;
}

int evalPosition(State& state, BoardValue player){
	const int WIN_SCORE 	=  1000;
	const int LOOSE_SCORE	= -1000;
	const int DRAW_SCORE	=  0;
	const int MOVE_SCORE	=  10;
	
	Board& board = state.board;
	
	int score;
	if (board.value == player){
		score = WIN_SCORE - MOVE_SCORE * state.move_num;
	}
	else if (board.value == nextPlayer(player)){
		score = LOOSE_SCORE + MOVE_SCORE * state.move_num;
	}
	else if (board.value == Draw){
		score = DRAW_SCORE;
	}
	else{
		if (USE_EVAL_POS){
			int my_score 	= calcBoardScore(board, player, 			  WIN_SCORE);
			int enemy_score = calcBoardScore(board, nextPlayer(player), WIN_SCORE);
			score = my_score - enemy_score;
			
			#ifdef LOG_EVAL_POS
				string s;
				cout << endl << endl
						<< "evalPosition() : "
						<< endl << endl;
					//cin >> s;
				cout << "my_score:    " << my_score 	<< endl;
				cout << "enemy_score: " << enemy_score 	<< endl;
				cout << "score:       " << score 		<< endl;
				cout << "evalPosition() end.";
				cin >> s;
			#endif
		}
		else{
			score = 0;
		}
	}
	return score;
}

void printPlyStatus(PlyStatus ply){
	const int PROGRESS_BAR_LENGTH = 20;
	
	int progress_bar_pos = PROGRESS_BAR_LENGTH * ply.moves_done / ply.moves_total;
	for (int i=0; i<progress_bar_pos; ++i){
		cout << "=";
	}
	cout << " " << ply.moves_done << "/" << ply.moves_total << endl;
	
	cout << "Player: " << valueToChar(ply.player) << " "
		 << "best_move: (" 	<< ply.best_move.first << " " << ply.best_move.second << ")  "
		 << "best_score: "	<< ply.best_score << "  "
		 << "current_move: (" << ply.current_move.first << " " << ply.current_move.second << " )"
		 << endl << endl;
}

void printMinimaxStatus(MinimaxStatus status, State& state){
	system("clear");
	printGameState(state);
	cout << endl << endl;
	
	vector<PlyStatus> plyes = status.plyes;
	int first_ply_to_print = max(0, (int)plyes.size()-7); // print only last 7 plyes
	for (int i = first_ply_to_print; i<plyes.size(); ++i){
		cout << "ply " << i << ": ";
		printPlyStatus(plyes[i]);
	}
}

pair<Move,int> minimax(State& state, BoardValue player, int ply, int alpha, int beta){
	Moves& moves = generateMoves(state);
	
	pair<Move,int> best_move;
	best_move.first = getEmptyMove();
	best_move.second = -10000;
	
	#ifdef LOG_MINIMAX
		PlyStatus ply_status;
		ply_status.player 		= player;
		ply_status.moves_done 	= 0;
		ply_status.moves_total 	= moves.size();
		ply_status.best_move	= best_move.first;
		ply_status.best_score	= best_move.second;
		minimax_status.plyes.push_back(ply_status);
	#endif
	
	for (Moves::iterator move = moves.begin(); move != moves.end(); ++move){
		int score;
		makeMove(state, state.board, *move, player);
		
		#ifdef LOG_MINIMAX
			minimax_status.plyes.back().current_move = *move;
			++minimax_status.plyes.back().moves_done; // processed moves count
			printMinimaxStatus(minimax_status, board);
			usleep(50*1000);
		#endif
		
		if (isGameEnded(state) || ply == 0){
			score = evalPosition(state, player);
			
			#ifdef LOG_MINIMAX
				//usleep(500*1000);
			#endif
		}
		else{
			pair<Move,int> response = minimax(state, nextPlayer(player), ply-1, -beta, -alpha);
			score = -response.second;
		}
		unmakeLastMove(state, state.board);
		
		if (score > best_move.second){
			best_move.first 	= *move;
			best_move.second 	= score;
			
			alpha = max(alpha, score);
			if (alpha >= beta) break;
			
			#ifdef LOG_MINIMAX
				minimax_status.plyes.back().best_move	= best_move.first;
				minimax_status.plyes.back().best_score	= best_move.second;
			#endif
		}
	}
	delete(&moves);
	
	#ifdef LOG_MINIMAX
		minimax_status.plyes.pop_back(); // we're done with this ply
	#endif
		
	return best_move;
}

Move findBestMove(State& state, int max_ply = INT_MAX){
	minimax_status.positions_done = 0;
	minimax_status.plyes.clear();
	
	pair<Move,int> move = minimax(state, state.current_player, max_ply, -INT_MAX, INT_MAX);
	
	return move.first;
}

void playTestGame(State& state){
	while (!isGameEnded(state)){
		printGameState(state);
				
		//string s; cin >> s; // debug
		usleep(250*1000);
		
		Move move;
		if (state.current_player == Cross){
			//move = getRandomMove(state);
			USE_EVAL_POS = false;
			move = findBestMove(state, 2);
		}
		else {
			USE_EVAL_POS = true;
			move = findBestMove(state, 2);
		}
		makeMove(state, state.board, move, state.current_player);
		state.current_player = nextPlayer(state.current_player);
		
		system("clear");
	}
	system("clear");
	printGameState(state);
}

State& getNewState(){
	State& state = *(new State);
	state.moves.clear();
	state.move_num = 0;
	state.current_player = Cross;
	state.board = getEmptyBoard();
	initSubboards(state.board);
	return state;
}

State& getNewState(Moves& moves){
	State& state = getNewState();
	for (int i=0; i<moves.size(); ++i){
		Move move = moves[i];
		makeMove(state, state.board, move, state.current_player);
		state.current_player = nextPlayer(state.current_player);
	}
	return state;
}

void testBoardLogic(){
	cout << "Create new state" << endl;
	State& state = getNewState();
	cout << "Print board:" << endl << endl;
	printBoard(state.board);
	cout << endl << endl;
	cout << "printBoardByLevel 1:" << endl << endl;
	printBoardByLevel(state.board, 1);
	cout << endl << endl;
	
	cout << "Create empty grid:" << endl << endl;
	Grid grid = getEmptyGrid();
	printGrid(grid);
	cout << endl << endl;
	
	cout << "Add 20 random moves:" << endl << endl;
	Moves moves;
	for (int i=0; i<20; ++i){
		moves.push_back(getRandomMove(state));
	}
	printMoves(moves);
	cout << endl << endl;
	
	cout << "addMovesToGrid:" << endl << endl;
	addMovesToGrid(moves, grid);
	printGrid(grid);
	cout << endl << endl;
	
	cout << "Apply moves to board:" << endl << endl;
	BoardValue player = Cross;
	for (Moves::iterator move = moves.begin(); move != moves.end(); ++move){
		makeMove(state, state.board, *move, player);
		player = nextPlayer(player);
	}
	
	cout << "printBoardByLevel 1:" << endl << endl;
	printBoardByLevel(state.board, 1);
	cout << endl << endl;
	cout << "printBoardByLevel 0:" << endl << endl;
	printBoardByLevel(state.board, 0);
	cout << endl << endl;
	
	cout << "Create empty grid2:" << endl << endl;
	Grid grid2 = getEmptyGrid();
	printGrid(grid2);
	cout << endl << endl;
	
	cout << "Board to grid2:" << endl << endl;
	boardToGrid(state.board, grid2);
	printGrid(grid2);
	cout << endl << endl;
}

void testGameLogic(){
	clock_t start = clock();

	State& state = getNewState();
	playTestGame(state);
	
	clock_t finish = clock();
	cout << "Time elapsed: " << (double)(finish-start) / CLOCKS_PER_SEC << " sec." << endl << endl;
	
	cout << "Now we'll unmake 10 moves" << endl << endl;
	string s; cin >> s;
	for (int i=0; i<10; ++i){
		usleep(500*1000);
		system("clear");
		unmakeLastMove(state, state.board);
		printGameState(state);
	}
	cout << "Now we'll try to resume game: " << endl << endl;
	cin >> s;
	playTestGame(state);
}

State& loadState(){
	Moves prev_moves;
	ifstream save_file(SAVE_FILENAME);
	if (save_file){
		prev_moves = loadMoves(save_file);
	}
	save_file.close();
	State& state = getNewState(prev_moves);
	return state;
}

void saveState(State& state){
	ofstream save_file(SAVE_FILENAME);
	save_file.clear();
	saveMoves(state.moves, save_file);
	save_file.close();
}

Move getAbsentMove(Grid& grid, Moves& moves){
	for (int row=0; row<GRID_HEIGHT; ++row){
		for (int col=0; col<GRID_WIDTH; ++col){
			if (grid[row][col] != Empty){
				Move move;
				move.first = row;
				move.second = col;
				if (find(moves.begin(), moves.end(), move) == moves.end()){
					return move;
				}
			}
		}
	}
	// there are no untracked moves;
	// happens only at the beginning of the game if we play as X
	return getEmptyMove(); 
}

// TODO: generalize, now works only for LEVELS=2
void issueOrder(Move move){
	int row, col;
	// subboard coords
	row = move.first  / BOARD_HEIGHT;
	col = move.second /	BOARD_WIDTH;
	cout << row << " " << col << " ";
	// cell coords
	row = move.first  % BOARD_HEIGHT;
	col = move.second %	BOARD_WIDTH;
	cout << row << " " << col;
}

void playRealGame(){
	// get current input
	char player;
	cin >> player;
	int row, col;
	cin >> row >> col;
	Grid& grid = readGrid();
	// merge with saved info
	State& state = loadState();
	Move opponents_move = getAbsentMove(grid, state.moves);
	if (opponents_move != getEmptyMove()){
		makeMove(state, state.board, opponents_move, state.current_player);
		state.current_player = nextPlayer(state.current_player);
	}
	// make a new move
	Move my_best_move = findBestMove(state, 4);
	makeMove(state, state.board, my_best_move, state.current_player);
	saveState(state);
	issueOrder(my_best_move);
}

void testFileIO(){
	ifstream input("input");
	// read data
	char player;
	input >> player;
	int row, col;
	input >> row >> col;
	Grid& grid = readGrid(input);
	// print data to stdout
	cout << player << endl;
	cout << row << " " << col << endl;
	printGrid(grid);
}

int main(){
	//system("clear");
	
	playRealGame();
	
	//srand(12); // used only for RandomPlayer

	//testBoardLogic();
	//cout << "Board Logic Test finished..." << endl;
	
	//testGameLogic();
	//cout << "Game Logic Test finished." << endl;
	
	//testFileIO();
	//cout << endl << "File I/O Test finished." << endl;
	
    //int s; cin >> s;
    
	return 0;
}