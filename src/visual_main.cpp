#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
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
	int draw_area_width;
	int draw_area_height;
	Board *board;
	cairo_surface_t *devices_surface, *printables_surface;
	GtkWidget *window, *mwindow, *grid, *bwindow, *swindow, *cwindow, *draw_area;
};

static gboolean on_draw_event(GtkWidget *, cairo_t *, State *);
static void on_resize_event(GtkWidget *, State *);

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
	uint16_t outputs[36], output_left, output_right;

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
	state.draw_area_width = std::max(700, 10 + 48 * boards[0].width);
	state.draw_area_height = std::max(575, 10 + 48 * boards[0].height);
	state.board = &boards[0];
	state.devices_surface = create_devices_surface();
	state.printables_surface = create_printables_surface();
	state.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	state.mwindow = gtk_scrolled_window_new(NULL, NULL);
	state.grid = gtk_grid_new();
	state.bwindow = gtk_scrolled_window_new(NULL, NULL);
	state.draw_area = gtk_drawing_area_new();

	g_signal_connect(G_OBJECT(state.draw_area), "draw", G_CALLBACK(on_draw_event), &state);
	g_signal_connect(state.window, "check-resize", G_CALLBACK(on_resize_event), &state);
	g_signal_connect(state.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
	gtk_window_set_position(GTK_WINDOW(state.window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(state.window), 800, 600);
	gtk_window_set_title(GTK_WINDOW(state.window), "Visual Marbelous");
	gtk_widget_set_size_request(state.bwindow, 700, 575);
	gtk_widget_set_size_request(state.draw_area, state.draw_area_width, state.draw_area_height);
	gtk_container_add(GTK_CONTAINER(state.bwindow), state.draw_area);
	gtk_grid_attach(GTK_GRID(state.grid), state.bwindow, 0, 0, 1, 1);
	// extra scrolled window to allow downsizing
	gtk_container_add(GTK_CONTAINER(state.mwindow), state.grid);
	gtk_container_add(GTK_CONTAINER(state.window), state.mwindow);

	gtk_widget_show_all(state.window);

	gtk_main();

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

static gboolean on_draw_event(GtkWidget *, cairo_t *cr, State *state){
	int w = state->board->width * 48, h = state->board->height * 48;
	int offx = (state->draw_area_width - w)/2, offy = (state->draw_area_height - h)/2;

	// bg
	cairo_rectangle(cr, 0.0, 0.0, state->draw_area_width, state->draw_area_height);
	cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
	cairo_fill(cr);

	// grid
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_set_line_width(cr, 0.5);
	for(int x = 0; x <= state->board->width; ++x){
		cairo_move_to(cr, offx + 48 * x, offy);
		cairo_line_to(cr, offx + 48 * x, offy + h);
	}
	for(int y = 0; y <= state->board->height; ++y){
		cairo_move_to(cr, offx, offy + 48 * y);
		cairo_line_to(cr, offx + w, offy + 48 * y);
	}
	cairo_stroke(cr);

	// devices
	for(int y = 0; y < state->board->height; ++y){
		for(int x = 0; x < state->board->width; ++x){
			int index = state->board->index(x, y);
			if(state->board->cells[index].device != DV_BOARD)
				draw_device(cr, state->devices_surface, state->board->cells[index], offx + 48 * x, offy + 48 * y);
			else{
				int startx = state->board->cells[index].board_call->x;
				std::string text = state->board->cells[index].board_call->board->actual_name;
				draw_board_call_cell(cr, state->printables_surface, text.substr((x - startx) * 2, 2), offx + 48 * x, offy + 48 * y);
				// draw box if first cell
				if(startx == x){
					cairo_rectangle(cr, offx + 48 * x + 1, offy + 48 * y + 1, 24 * text.length() - 1, 46);
					cairo_set_source_rgb(cr, 1, 0.2, 0.5);
					cairo_set_line_width(cr, 3);
					cairo_stroke(cr);
				}
			}
		}
	}

	return FALSE;
}

static void on_resize_event(GtkWidget *window, State *state){
	int width, height;
	gtk_window_get_size(GTK_WINDOW(window), &width, &height);
	gtk_widget_set_size_request(state->bwindow, width - 100, height - 25);

	state->draw_area_width = std::max(width - 100, 10 + 48 * state->board->width);
	state->draw_area_height = std::max(height - 25, 10 + 48 * state->board->height);
	gtk_widget_set_size_request(state->draw_area, state->draw_area_width, state->draw_area_height);
}
