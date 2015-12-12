#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stack>
#include <string>
#include <sstream>
#include <vector>

#include <gtk/gtk.h>
#include <cairo.h>

#include "board.h"
#include "emit.h"
#include "io_functions.h"
#include "load.h"
#include "options.h"
#include "surfaces.h"

option::Option *options;
int verbosity;
bool cylindrical;

struct State {
	int width, height;
	int draw_area_width;
	int draw_area_height;
	int swindow_height;
	BoardCall::RunState *rs; // current runstate
	// stack of runstates...
	std::stack<BoardCall::RunState *> rs_stack;
	cairo_surface_t *devices_surface, *printables_surface, *marble_surface, *cn16_surface;
	GtkWidget *window, *mwindow, *grid, *lgrid, *bwindow, *swindow, *cgrid, *draw_area, *sdraw_area;
	GtkWidget *play_toggle, *tick_once;
	cairo_surface_t *swindow_surface;

	int movement_frame; // 0 = ticking, 1+ = marbles are moving
	bool autoplay;
};

static void start_board_movement(State *);
static gboolean on_bdraw_event(GtkWidget *, cairo_t *, State *);
static gboolean on_sdraw_event(GtkWidget *, cairo_t *, State *);
static void on_resize_event(GtkWidget *, State *);
static gboolean tick_board(State *);

static void play_toggle_clicked(GtkButton *, State *);
static void tick_once_clicked(GtkButton *, State *);

int main(int argc, char *argv[]){
	// process arguments
	option::Stats stats(usage, argc - 1, argv + 1);
	option::Option _options[stats.options_max], buffer[stats.buffer_max];
	option::Parser parse(usage, argc - 1, argv + 1, _options, buffer);

	options = _options;

	if(parse.error())
		return -1;
	
	if(options[OPT_HELP] || argc == 1){
		option::printUsage(std::cout, usage);
		return 0;
	}

	for(option::Option *opt = options[OPT_UNKNOWN]; opt; opt = opt->next())
		emit_warning(std::string("Unknown option: ") + opt->name);

	if(parse.nonOptionsCount() == 0){
		emit_error("No input file");
		return -2;
	}

	std::string filename = parse.nonOption(0);
	// load
	std::srand(std::time(nullptr));
	prepare_io(true);
	std::vector<Board> boards;
	std::map<std::string, unsigned> lookup;
	if(!load_mbl_file(filename, boards, lookup)){
		emit_error("Could not load file " + filename);
		return -3;
	}

	// get highest input
	int highest_input = -1;
	for(int i = 36; i --> 0;){
		if(!boards[0].inputs[i].empty()){
			highest_input = i;
			break;
		}
	}

	// check arguments
	if(parse.nonOptionsCount() != 2 + highest_input){ // filename + (highest_input + 1)
		emit_error("Expected " + std::to_string(highest_input + 1) + " inputs, got " + std::to_string(parse.nonOptionsCount() - 1));
		return -4;	
	}

	// misc options
	verbosity = options[OPT_VERBOSE].count();
	cylindrical = (options[OPT_CYLINDRICAL].last()->type() == OPT_TYPE_ENABLE);

	BoardCall bc{&boards[0], 0, 0};
	uint8_t inputs[36] = { 0 };

	for(int i = 0; i <= highest_input; ++i){
		if(!boards[0].inputs[i].empty()){
			std::string opt = parse.nonOption(i + 1);
			unsigned value = 0;
			bool too_large = false;
			for(auto c : opt){
				if(std::isdigit(c)){
					value = 10 * value + (c - '0');
					if(value > 255)
						too_large = true;
				}else{
					emit_error("Argument value " + opt + " is not a nonnegative integer");
				}
			}
			if(too_large){
				emit_warning("Argument value " + opt + " is larger than 255; using value mod 256");
			}
			inputs[i] = value & 255;
		}
	}

	// if verbose, stall printing to end..
	if(options[OPT_VERBOSE].count() > 0){
		stdout_write = _stdout_save;
	}

	// GTK window setup
	gtk_init(nullptr, nullptr);

	State state;
	// use largest board width instead
	state.width = 800;
	state.height = 600;
	state.draw_area_width = std::max(700, 10 + 48 * (boards[0].width + 2));
	state.draw_area_height = std::max(550, 10 + 48 * (boards[0].height + 1));
	state.swindow_height = 16;
	state.rs = bc.new_run_state(inputs);
	state.devices_surface = create_devices_surface();
	state.printables_surface = create_printables_surface();
	state.marble_surface = create_marble_surface();
	state.cn16_surface = create_cn16_surface();
	state.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	state.mwindow = gtk_scrolled_window_new(NULL, NULL);
	state.grid = gtk_grid_new();
	state.lgrid = gtk_grid_new();
	state.bwindow = gtk_scrolled_window_new(NULL, NULL);
	state.swindow = gtk_scrolled_window_new(NULL, NULL);
	state.cgrid = gtk_grid_new();
	state.draw_area = gtk_drawing_area_new();
	state.sdraw_area = gtk_drawing_area_new();
	state.swindow_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 90, state.swindow_height);
	state.movement_frame = -1;
	state.autoplay = false;

	state.play_toggle = gtk_button_new_with_mnemonic("_Play");
	state.tick_once = gtk_button_new_with_mnemonic("_Tick");

	// draw "MB" to swindow_surface...
	cairo_t *tmp = cairo_create(state.swindow_surface);
	draw_text_cn16(tmp, state.cn16_surface, "MB", 0, 0);
	cairo_destroy(tmp);

	g_signal_connect(G_OBJECT(state.play_toggle), "clicked", G_CALLBACK(play_toggle_clicked), &state);
	g_signal_connect(G_OBJECT(state.tick_once), "clicked", G_CALLBACK(tick_once_clicked), &state);
	
	g_signal_connect(G_OBJECT(state.draw_area), "draw", G_CALLBACK(on_bdraw_event), &state);
	g_signal_connect(G_OBJECT(state.sdraw_area), "draw", G_CALLBACK(on_sdraw_event), &state);
	g_signal_connect(state.window, "check-resize", G_CALLBACK(on_resize_event), &state);
	g_signal_connect(state.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_grid_attach(GTK_GRID(state.cgrid), state.play_toggle, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(state.cgrid), state.tick_once, 1, 0, 2, 1);

	gtk_window_set_position(GTK_WINDOW(state.window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(state.window), 800, 600);
	gtk_window_set_title(GTK_WINDOW(state.window), "Visual Marbelous");
	gtk_widget_set_size_request(state.bwindow, 700, 572);
	gtk_widget_set_size_request(state.draw_area, state.draw_area_width, state.draw_area_height);
	gtk_widget_set_size_request(state.swindow, 100, 600);
	gtk_container_add(GTK_CONTAINER(state.bwindow), state.draw_area);
	gtk_container_add(GTK_CONTAINER(state.swindow), state.sdraw_area);
	gtk_grid_attach(GTK_GRID(state.lgrid), state.bwindow, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(state.lgrid), state.cgrid, 0, 1, 1, 2);
	gtk_grid_attach(GTK_GRID(state.grid), state.lgrid, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(state.grid), state.swindow, 1, 0, 2, 1);
	// extra scrolled window to allow downsizing
	gtk_container_add(GTK_CONTAINER(state.mwindow), state.grid);
	gtk_container_add(GTK_CONTAINER(state.window), state.mwindow);

	gtk_widget_show_all(state.window);

	gtk_main();

	// cleanup
	delete state.rs;
	cairo_surface_destroy(state.marble_surface);
	cairo_surface_destroy(state.printables_surface);
	cairo_surface_destroy(state.devices_surface);

	return 0;

	// BoardCall::RunState *rs = bc.call(inputs);

	// if(options[OPT_VERBOSE].count() > 0){
	// 	std::fputs("Combined STDOUT: ", stdout);
	// 	for(uint8_t c : stdout_get_saved()){
	// 		_stdout_writehex(c);
	// 	}
	// 	std::fputc('\n', stdout);
	// }

	// prepare_io(false);

	// int res = (rs->outputs[0] >> 8) ? rs->outputs[0] & 0xFF : 0;

	// delete rs;
}

static void start_board_movement(State *state){
	state->movement_frame = 0;
	g_timeout_add(60, GSourceFunc(tick_board), state);
}

static gboolean on_bdraw_event(GtkWidget *, cairo_t *cr, State *state){
	const Board *board = state->rs->bc->board;
	int w = board->width * 48, h = board->height * 48;
	int offx = (state->draw_area_width - w)/2, offy = (state->draw_area_height - h)/2;

	// bg
	cairo_rectangle(cr, 0.0, 0.0, state->draw_area_width, state->draw_area_height);
	cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
	cairo_fill(cr);

	// grid
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_set_line_width(cr, 0.5);
	for(int x = 0; x <= board->width; ++x){
		cairo_move_to(cr, offx + 48 * x, offy);
		cairo_line_to(cr, offx + 48 * x, offy + h);
	}
	for(int y = 0; y <= board->height; ++y){
		cairo_move_to(cr, offx, offy + 48 * y);
		cairo_line_to(cr, offx + w, offy + 48 * y);
	}
	cairo_stroke(cr);

	// devices
	for(int y = 0; y < board->height; ++y){
		for(int x = 0; x < board->width; ++x){
			int index = board->index(x, y);
			if(board->cells[index].device != DV_BOARD)
				draw_device(cr, state->devices_surface, board->cells[index], offx + 48 * x, offy + 48 * y);
			else{
				int startx = board->cells[index].board_call->x;
				std::string text = board->cells[index].board_call->board->actual_name;
				draw_board_call_cell(cr, state->printables_surface, text.substr((x - startx) * 2, 2), offx + 48 * x, offy + 48 * y);
				// draw box if first cell
				if(startx == x){
					cairo_new_path(cr);
					cairo_rectangle(cr, offx + 48 * x + 1, offy + 48 * y + 1, 24 * text.length() - 1, 46);
					cairo_set_source_rgb(cr, 1, 0.2, 0.5);
					cairo_set_line_width(cr, 3);
					cairo_stroke(cr);
				}
			}
		}
	}

	// marbles
	if(state->movement_frame == 0){
		for(int y = 0; y < board->height; ++y){
			for(int x = 0; x < board->width; ++x){
				int index = board->index(x, y);
				uint16_t value = state->rs->cur_marbles[index];
				if(value & 0xFF00)
					draw_marble(cr, state->printables_surface, state->marble_surface, value & 0xFF, offx + 48 * x, offy + 48 * y);
			}
		}
	}else{
		// draw marbles in movement...
		const double offset = 48.0 / 5;
		for(const auto m : state->rs->moved_marbles){
			uint16_t dir = (m.first & 0xFF00) >> 8;
			uint16_t x = offx + 48 * (m.second % board->width);
			uint16_t y = offy + 48 * (m.second / board->width);
			uint8_t value = (m.first & 0xFF);
			if(dir == 0) // no move
				draw_marble(cr, state->printables_surface, state->marble_surface, value, x, y); 
			else if(dir == 1) // left
				draw_marble(cr, state->printables_surface, state->marble_surface, value, x - offset * state->movement_frame, y);
			else if(dir == 2) // right
				draw_marble(cr, state->printables_surface, state->marble_surface, value, x + offset * state->movement_frame, y);
			else // down
				draw_marble(cr, state->printables_surface, state->marble_surface, value, x, y + offset * state->movement_frame);
		}
	}
	return FALSE;
}

static gboolean on_sdraw_event(GtkWidget *, cairo_t *cr, State *state){
	cairo_new_path(cr);
	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgb(cr, 0.2, 0.2, 0.3);
	cairo_rectangle(cr, 0, 0, 100, state->height);
	cairo_fill(cr);

	cairo_new_path(cr);
	cairo_set_line_width(cr, 0);
	cairo_rectangle(cr, 5, 5, 90, state->swindow_height);
	cairo_set_source_surface(cr, state->swindow_surface, 5, 5);
	cairo_paint(cr);

	cairo_new_path(cr);
	cairo_set_line_width(cr, 1.0);
	cairo_set_source_rgb(cr, 0.9, 1.0, 0.9);
	cairo_rectangle(cr, 4, 4, 91, 17);
	cairo_stroke(cr);

	return FALSE;
}

static void on_resize_event(GtkWidget *window, State *state){
	gtk_window_get_size(GTK_WINDOW(window), &state->width, &state->height);
	gtk_widget_set_size_request(state->bwindow, state->width - 100, state->height - 28);
	gtk_widget_set_size_request(state->swindow, 100, state->height);

	state->draw_area_width = std::max(state->width - 100, 10 + 48 * (state->rs->bc->board->width + 2));
	state->draw_area_height = std::max(state->height - 28, 10 + 48 * (state->rs->bc->board->height + 1));
	gtk_widget_set_size_request(state->draw_area, state->draw_area_width, state->draw_area_height);
}

static gboolean tick_board(State *state){
	bool not_finished;
	if(state->movement_frame == -1){
		return false;
	}else if(state->movement_frame == 5){
		state->movement_frame = 0;
		state->rs->moved_marbles.clear();
		if(!state->autoplay){
			state->movement_frame = -1;
			return false;
		}
	}

	if(state->movement_frame != 0){
		++state->movement_frame;
		not_finished = true;
	}else if(state->rs->prepared_board_calls.size() != 0){
		state->rs_stack.push(state->rs);
		state->rs = state->rs->prepared_board_calls[0];

		cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 90, state->swindow_height + 18);
		cairo_t *tmp = cairo_create(surf);
		// copy over
		cairo_set_line_width(tmp, 0);
		cairo_rectangle(tmp, 0, 18, 90, state->swindow_height);
		cairo_set_source_surface(tmp, state->swindow_surface, 0, 18);
		cairo_paint(tmp);
		// add new board
		draw_text_cn16(tmp, state->cn16_surface, state->rs->bc->board->short_name.substr(0, 7), 0, 0);
		// cleanup
		cairo_destroy(tmp);
		cairo_surface_destroy(state->swindow_surface);
		state->swindow_surface = surf;
		state->swindow_height += 18;
		gtk_widget_queue_draw(state->sdraw_area);

		not_finished = true;
	}else if(state->rs->is_finished()){
		state->rs->finalize();
		if(state->rs_stack.size() == 0){
			cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 90, state->swindow_height - 16);
			cairo_t *tmp = cairo_create(surf);
			cairo_surface_t *ssurf = cairo_surface_create_for_rectangle(state->swindow_surface, 0, 16, 90, state->swindow_height);
			cairo_set_source_surface(tmp, ssurf, 0, 0);
			cairo_paint(tmp);
			cairo_surface_destroy(ssurf);
			cairo_destroy(tmp);
			cairo_surface_destroy(state->swindow_surface);
			state->swindow_surface = surf;
			state->swindow_height -= 16;
			gtk_widget_queue_draw(state->sdraw_area);

			not_finished = false;
		}else{
			cairo_surface_t *surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 90, state->swindow_height - 18);
			cairo_t *tmp = cairo_create(surf);
			cairo_surface_t *ssurf = cairo_surface_create_for_rectangle(state->swindow_surface, 0, 18, 90, state->swindow_height);
			cairo_set_source_surface(tmp, ssurf, 0, 0);
			cairo_paint(tmp);
			cairo_surface_destroy(ssurf);
			cairo_destroy(tmp);
			cairo_surface_destroy(state->swindow_surface);
			state->swindow_surface = surf;
			state->swindow_height -= 18;
			gtk_widget_queue_draw(state->sdraw_area);

			BoardCall::RunState *top = state->rs_stack.top();
			top->prepared_board_calls.erase(top->prepared_board_calls.begin());
			top->processed_board_calls.push_back(state->rs);
			state->rs_stack.pop();
			state->rs = top;
			not_finished = true;
		}
	}else if(state->rs->processed_board_calls.size() != 0){
		state->rs->tick();
		++state->movement_frame;
		not_finished = true;
	}else{
		state->rs->prepare_board_calls();
		if(state->rs->prepared_board_calls.size() == 0)
			state->rs->tick(), ++state->movement_frame;
		not_finished = true;
	}
	std::fflush(stdout);
	gtk_widget_queue_draw(state->draw_area);

	return not_finished ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

static void play_toggle_clicked(GtkButton *button, State *state){
	if(!state->autoplay){
		state->autoplay = true;
		start_board_movement(state);
		gtk_button_set_label(button, "_Pause");
	}else{
		state->autoplay = false;
		gtk_button_set_label(button, "_Play");
	}
}

static void tick_once_clicked(GtkButton *button, State *state){
	if(state->movement_frame == -1 && !state->autoplay){
		start_board_movement(state);
	}
}
