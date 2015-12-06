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

struct gobject {
	int draw_area_width;
	int draw_area_height;
	Board *board;
	cairo_surface_t *devices_surface, *printables_surface;
};

static gboolean on_draw_event(GtkWidget *, cairo_t *, gobject *);

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

	// gobject
	gobject gobj;
	// use largest board width instead
	gobj.draw_area_width = std::max(700, 10 + 48 * boards[0].width);
	gobj.draw_area_height = std::max(575, 10 + 48 * boards[0].height);
	gobj.board = &boards[0];
	gobj.devices_surface = create_devices_surface();
	gobj.printables_surface = create_printables_surface();
	// gobj.board_symbol

	// GTK window
	GtkWidget *window, *grid, *bwindow, *swindow, *cwindow, *viewport, *draw_area;
	gtk_init(nullptr, nullptr);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	grid = gtk_grid_new();
	bwindow = gtk_scrolled_window_new(NULL, NULL);

	// swindow = gtk_scrolled_window_new(NULL, NULL);
	// cwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	viewport = gtk_viewport_new(NULL, NULL);
	draw_area = gtk_drawing_area_new();

	g_signal_connect(G_OBJECT(draw_area), "draw", G_CALLBACK(on_draw_event), &gobj);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
	gtk_window_set_title(GTK_WINDOW(window), "Visual Marbelous");

	gtk_widget_set_size_request(bwindow, 700, 575);
	gtk_widget_set_size_request(draw_area, gobj.draw_area_width, gobj.draw_area_height);
	gtk_container_add(GTK_CONTAINER(viewport), draw_area);
	gtk_container_add(GTK_CONTAINER(bwindow), viewport);
	gtk_grid_attach(GTK_GRID(grid), bwindow, 0, 1, 1, 2);
	gtk_container_add(GTK_CONTAINER(window), grid);

	gtk_widget_show_all(window);

	gtk_main();

	return 0;
	// bc.call(inputs, outputs, output_left, output_right);

	// if(options[OPT_VERBOSE].count() > 0){
	// 	std::fputs("Combined STDOUT: ", stdout);
	// 	for(uint8_t c : stdout_get_saved()){
	// 		_stdout_writehex(c);
	// 	}
	// 	std::fputc('\n', stdout);
	// }

	// prepare_io(false);

	// return (outputs[0] >> 8) ? outputs[0] & 0xFF : 0;
}

static gboolean on_draw_event(GtkWidget *, cairo_t *cr, gobject *gobj){
	int w = gobj->board->width * 48, h = gobj->board->height * 48;
	int offx = (gobj->draw_area_width - w)/2, offy = (gobj->draw_area_height - h)/2;

	// bg
	cairo_rectangle(cr, 0.0, 0.0, gobj->draw_area_width, gobj->draw_area_height);
	cairo_set_source_rgb(cr, 0.15, 0.15, 0.15);
	cairo_fill(cr);

	// grid
	cairo_set_source_rgb(cr, 1, 1, 1);
	cairo_set_line_width(cr, 0.5);
	for(int x = 0; x <= gobj->board->width; ++x){
		cairo_move_to(cr, offx + 48 * x, offy);
		cairo_line_to(cr, offx + 48 * x, offy + h);
	}
	for(int y = 0; y <= gobj->board->height; ++y){
		cairo_move_to(cr, offx, offy + 48 * y);
		cairo_line_to(cr, offx + w, offy + 48 * y);
	}
	cairo_stroke(cr);

	// devices
	for(int y = 0; y < gobj->board->height; ++y){
		for(int x = 0; x < gobj->board->width; ++x){
			int index = gobj->board->index(x, y);
			if(gobj->board->cells[index].device != DV_BOARD)
				draw_device(cr, gobj->devices_surface, gobj->board->cells[index], offx + 48 * x, offy + 48 * y);
			else{
				int startx = gobj->board->cells[index].board_call->x;
				std::string text = gobj->board->cells[index].board_call->board->actual_name;
				draw_board_call_cell(cr, gobj->printables_surface, text.substr((x - startx) * 2, 2), offx + 48 * x, offy + 48 * y);
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
