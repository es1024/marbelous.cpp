#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <deque>
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
int verbosity = 0;
bool cylindrical;

struct State {
	int width, height;
	int draw_area_width;
	int draw_area_height;
	int swindow_height;
	BoardCall::RunState *rs; // current runstate (being shown)
	// stack of runstates...
	std::deque<BoardCall::RunState *> rs_stack;
	cairo_surface_t *devices_surface, *printables_surface, *marble_surface, *cn16_surface;
	GtkWidget *window, *mwindow, *grid, *lgrid, *bwindow, *swindow, *cgrid, *draw_area, *sdraw_area;
	GtkWidget *stdout_window, *stdout_label;
	GtkWidget *play_toggle, *tick_once, *finish;
	cairo_surface_t *swindow_surface;

	std::string pstdout;

	int active_frame;

	int movement_frame; // 0 = ticking, 1+ = marbles are moving
	bool autoplay;

	// settings
	bool edit_in_hex;
};

// callbacks used by prompt_marble_value
static gboolean marble_edit_changed(GtkWidget *, GdkEvent *, std::tuple<State *, GtkWidget *, int> *info){
	State *state = std::get<0>(*info);
	GtkWidget *text_area = std::get<1>(*info);
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(text_area));

	// read char by char to handle overflow appropriately
	int value = 0;
	while(*text){
		char c = *text;
		if(state->edit_in_hex){
			if('0' <= c && c <= '9')
				value = (value * 16 + (c - '0')) % 256;
			else if('A' <= c && c <= 'F')
				value = (value * 16 + (c - 'A' + 10)) % 256;
			else if('a' <= c && c <= 'f')
				value = (value * 16 + (c - 'a' + 10)) % 256;
			else break;
		}else{
			if(std::isdigit(c))
				value = (value * 10 + (c - '0')) % 256;
			else break;
		}
		++text;
	}

	// update display with input mod 256
	std::ostringstream ss;
	if(state->edit_in_hex)
		ss << std::hex;
	ss << value;
	gtk_entry_set_text(GTK_ENTRY(text_area), ss.str().c_str());

	// set value
	std::get<2>(*info) = value;

	return FALSE;
}
static void marble_edit_hex_changed(GtkToggleButton *, std::tuple<State *, GtkWidget *, int> *info){
	State *state = std::get<0>(*info);
	GtkWidget *text_area = std::get<1>(*info);
	const gchar *text = gtk_entry_get_text(GTK_ENTRY(text_area));

	// convert to hex/decimal
	int value = std::stoi(text, nullptr, (state->edit_in_hex ? 16 : 10));

	std::string out;
	if(!state->edit_in_hex){
		std::stringstream ss;
		ss << std::hex << value;
		out = ss.str();
	}else{
		out = std::to_string(value);
	}

	gtk_entry_set_text(GTK_ENTRY(text_area), out.c_str());

	// toggle
	state->edit_in_hex = !state->edit_in_hex;
}

static int prompt_marble_value(State *state, const char *title, int default_value = 0){
	GtkWidget *dialog, *carea, *varea, *text_area_label, *text_area, *hex_box;
	std::string default_text = "";

	{
		std::ostringstream ss;
		if(state->edit_in_hex)
			ss << std::hex;
		ss << default_value;

		default_text = ss.str();
	}

	dialog = gtk_dialog_new_with_buttons(title, GTK_WINDOW(state->window), GTK_DIALOG_DESTROY_WITH_PARENT, "_Cancel", GTK_RESPONSE_NONE, "_OK", GTK_RESPONSE_ACCEPT, NULL);
	carea = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

	// Marble value area
	text_area_label = gtk_label_new("Marble Value: ");
	text_area = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(text_area), default_text.c_str());

	varea = gtk_grid_new();
	gtk_grid_attach(GTK_GRID(varea), text_area_label, 0, 0, 1, 1);
	gtk_grid_attach_next_to(GTK_GRID(varea), text_area, text_area_label, GTK_POS_RIGHT, 1, 1);

	// Hex vs decimal area
	hex_box = gtk_check_button_new_with_label("Hexadecimal");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(hex_box), state->edit_in_hex);

	gtk_box_pack_start(GTK_BOX(carea), varea, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(carea), hex_box, TRUE, TRUE, 0);

	// signals
	std::tuple<State *, GtkWidget *, int> info{state, text_area, default_value};
	g_signal_connect(G_OBJECT(text_area), "focus-out-event", G_CALLBACK(marble_edit_changed), &info);
	g_signal_connect(G_OBJECT(hex_box), "toggled", G_CALLBACK(marble_edit_hex_changed), &info);

	gtk_widget_show_all(dialog);
	gint res = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_hide(dialog);
	gtk_widget_destroy(dialog);

	if(res == GTK_RESPONSE_ACCEPT)
		return std::get<2>(info);
	else return -1;
}

static void start_board_movement(State *);
static void stop_board_movement(State *);
static void flush_stdout(State *);
static gboolean on_bdraw_event(GtkWidget *, cairo_t *, State *);
static gboolean on_sdraw_event(GtkWidget *, cairo_t *, State *);
static void on_resize_event(GtkWidget *, State *);
static gboolean tick_board(State *);
static gboolean stack_clicked(GtkWidget *, GdkEvent *, State *);
static gboolean board_double_clicked(GtkWidget *, GdkEvent *event, State *state);
static void play_toggle_clicked(GtkButton *, State *);
static void tick_once_clicked(GtkButton *, State *);
static void finish_clicked(GtkButton *, State *);

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

	stdout_write = _stdout_save;	

	// GTK window setup
	gtk_init(nullptr, nullptr);

	State state;
	// use largest board width instead
	state.width = 800;
	state.height = 600;
	state.draw_area_width = std::max(700, 10 + 48 * (boards[0].width + 2));
	state.draw_area_height = std::max(544, 10 + 48 * (boards[0].height + 1));
	state.swindow_height = 16;
	state.rs = bc.new_run_state(inputs);
	state.rs_stack.push_front(state.rs);
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
	state.stdout_window = gtk_scrolled_window_new(NULL, NULL);
	state.stdout_label = gtk_label_new(NULL);
	state.pstdout = "";
	state.active_frame = 0;
	state.movement_frame = -1;
	state.autoplay = false;
	state.edit_in_hex = true;

	gtk_widget_set_size_request(state.stdout_window, 700, 48);
	gtk_widget_set_halign(state.stdout_label, GTK_ALIGN_START);
	gtk_label_set_selectable(GTK_LABEL(state.stdout_label), true);
	gtk_label_set_yalign(GTK_LABEL(state.stdout_label), 0.0);
	gtk_label_set_markup(GTK_LABEL(state.stdout_label), "<span font='Courier New 12'><b>STDOUT: </b>\n<span foreground='red'>_</span></span>");
	gtk_container_add(GTK_CONTAINER(state.stdout_window), state.stdout_label);

	state.play_toggle = gtk_button_new_with_mnemonic("_Play");
	state.tick_once = gtk_button_new_with_mnemonic("_Tick");
	state.finish = gtk_button_new_with_mnemonic("_Finish");

	// draw "MB" to swindow_surface...
	cairo_t *tmp = cairo_create(state.swindow_surface);
	draw_text_cn16(tmp, state.cn16_surface, "MB", 0, 0);
	cairo_destroy(tmp);

	g_signal_connect(G_OBJECT(state.play_toggle), "clicked", G_CALLBACK(play_toggle_clicked), &state);
	g_signal_connect(G_OBJECT(state.tick_once), "clicked", G_CALLBACK(tick_once_clicked), &state);
	g_signal_connect(G_OBJECT(state.finish), "clicked", G_CALLBACK(finish_clicked), &state);
	gtk_widget_add_events(state.sdraw_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(state.sdraw_area), "button-press-event", G_CALLBACK(stack_clicked), &state);
	gtk_widget_add_events(state.draw_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(state.draw_area), "button-press-event", G_CALLBACK(board_double_clicked), &state);
	
	g_signal_connect(G_OBJECT(state.draw_area), "draw", G_CALLBACK(on_bdraw_event), &state);
	g_signal_connect(G_OBJECT(state.sdraw_area), "draw", G_CALLBACK(on_sdraw_event), &state);
	g_signal_connect(state.window, "check-resize", G_CALLBACK(on_resize_event), &state);
	g_signal_connect(state.window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	gtk_grid_attach(GTK_GRID(state.cgrid), state.play_toggle, 0, 0, 1, 1);
	gtk_grid_attach_next_to(GTK_GRID(state.cgrid), state.tick_once, state.play_toggle, GTK_POS_RIGHT, 1, 1);
	gtk_grid_attach_next_to(GTK_GRID(state.cgrid), state.finish, state.tick_once, GTK_POS_RIGHT, 1, 1);

	gtk_window_set_position(GTK_WINDOW(state.window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size(GTK_WINDOW(state.window), 800, 600);
	gtk_window_set_title(GTK_WINDOW(state.window), "Visual Marbelous");
	gtk_widget_set_size_request(state.bwindow, 700, 524);
	gtk_widget_set_size_request(state.draw_area, state.draw_area_width, state.draw_area_height);
	gtk_widget_set_size_request(state.swindow, 100, 600);
	gtk_container_add(GTK_CONTAINER(state.bwindow), state.draw_area);
	gtk_container_add(GTK_CONTAINER(state.swindow), state.sdraw_area);
	gtk_grid_attach(GTK_GRID(state.lgrid), state.bwindow, 0, 0, 1, 1);
	gtk_grid_attach_next_to(GTK_GRID(state.lgrid), state.stdout_window, state.bwindow, GTK_POS_BOTTOM, 1, 1);
	gtk_grid_attach_next_to(GTK_GRID(state.lgrid), state.cgrid, state.stdout_window, GTK_POS_BOTTOM, 1, 1);
	gtk_grid_attach(GTK_GRID(state.grid), state.lgrid, 0, 0, 1, 1);
	gtk_grid_attach_next_to(GTK_GRID(state.grid), state.swindow, state.lgrid, GTK_POS_RIGHT, 1, 1);
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

	// int res = (rs->outputs[0] >> 8) ? rs->outputs[0] & 0xFF : 0;

	// delete rs;
}

static void start_board_movement(State *state){
	state->movement_frame = 0;
	g_timeout_add(60, GSourceFunc(tick_board), state);
}

static void stop_board_movement(State *state){
	state->movement_frame = -1;
	gtk_widget_set_sensitive(state->play_toggle, true);
	gtk_widget_set_sensitive(state->tick_once, true);
	gtk_widget_set_sensitive(state->finish, true);
	gtk_button_set_label(GTK_BUTTON(state->play_toggle), "_Play");
}

static void flush_stdout(State *state){
	const auto &outv = stdout_get_saved();
	if(outv.size() > state->pstdout.length()){
		for(int i = state->pstdout.length(), len = outv.size(); i < len; ++i)
			if(outv[i] == 0 || outv[i] > 0x7F)
				state->pstdout += 0x01; // gtk errors if extended ascii + null byte issues
			else
				state->pstdout += outv[i];
		char *out = g_markup_printf_escaped("<span font='Courier New 12'><b>STDOUT: </b>\n"
			"%s<span foreground='red'>_</span></span>", state->pstdout.c_str());

		gtk_label_set_markup(GTK_LABEL(state->stdout_label), out);
		GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(state->stdout_window));
		gtk_adjustment_set_value(adj, gtk_adjustment_get_upper(adj));

		g_free(out);
	}
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
	if(state->movement_frame <= 0 || state->movement_frame > 5){
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
	cairo_rectangle(cr, 4, 4 - 18 * state->active_frame, 91, 17);
	cairo_stroke(cr);

	return FALSE;
}

static void on_resize_event(GtkWidget *window, State *state){
	gtk_window_get_size(GTK_WINDOW(window), &state->width, &state->height);
	gtk_widget_set_size_request(state->bwindow, state->width - 100, state->height - 76);
	gtk_widget_set_size_request(state->swindow, 100, state->height);

	state->draw_area_width = std::max(state->width - 100, 10 + 48 * (state->rs->bc->board->width + 2));
	state->draw_area_height = std::max(state->height - 76, 10 + 48 * (state->rs->bc->board->height + 1));
	gtk_widget_set_size_request(state->draw_area, state->draw_area_width, state->draw_area_height);
}

static gboolean tick_board(State *state){
	bool not_finished;
	if(state->movement_frame == -1){
		return false;
	}else if(state->movement_frame == 5 || state->movement_frame == 10){
		state->movement_frame = 0;
		state->rs->moved_marbles.clear();
		if(!state->autoplay){
			stop_board_movement(state);
			return false;
		}
	}

	if(state->movement_frame != 0){
		++state->movement_frame;
		not_finished = true;
	}else if(state->rs->prepared_board_calls.size() != 0){
		state->movement_frame = 6;

		state->rs = state->rs->prepared_board_calls[0];
		state->rs_stack.push_front(state->rs);

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
		if(state->rs_stack.size() == 1){
			not_finished = false;
		}else{
			state->movement_frame = 6;
	
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

			state->rs_stack.pop_front();
			BoardCall::RunState *top = state->rs_stack.front();
			top->prepared_board_calls.erase(top->prepared_board_calls.begin());
			top->processed_board_calls.push_back(state->rs);
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
	flush_stdout(state);
	gtk_widget_queue_draw(state->draw_area);

	if(!not_finished)
		stop_board_movement(state);

	return not_finished ? G_SOURCE_CONTINUE : G_SOURCE_REMOVE;
}

static void play_toggle_clicked(GtkButton *button, State *state){
	if(!state->autoplay){
		state->autoplay = true;
		state->rs = state->rs_stack.front();
		state->active_frame = 0;
		gtk_widget_set_sensitive(state->tick_once, false);
		gtk_widget_set_sensitive(state->finish, false);
		gtk_button_set_label(button, "_Pause");
		gtk_widget_queue_draw(state->sdraw_area);
		start_board_movement(state);
	}else{
		state->autoplay = false;
		gtk_button_set_label(button, "_Play");
	}
}

static void tick_once_clicked(GtkButton *, State *state){
	if(state->movement_frame == -1 && !state->autoplay){
		state->rs = state->rs_stack.front();
		state->active_frame = 0;
		gtk_widget_set_sensitive(state->play_toggle, false);
		gtk_widget_set_sensitive(state->tick_once, false);
		gtk_widget_set_sensitive(state->finish, false);
		gtk_widget_queue_draw(state->sdraw_area);
		start_board_movement(state);
	}
}

static void finish_clicked(GtkButton *, State *state){
	if(state->movement_frame == -1 && !state->autoplay){
		// run to completion
		state->rs = state->rs_stack.front();
		state->active_frame = 0;
		while(state->rs->prepared_board_calls.size() > 0){
			BoardCall::RunState *rs = state->rs->prepared_board_calls[0];
			while(rs->tick(false));
			rs->finalize();

			state->rs->processed_board_calls.push_back(rs);
			state->rs->prepared_board_calls.erase(state->rs->prepared_board_calls.begin());
		}
		if(state->rs->processed_board_calls.size() > 0){
			if(state->rs->tick(true))
				while(state->rs->tick(false));
		}else{
			while(state->rs->tick(false));
		}
		state->rs->finalize();

		flush_stdout(state);
		gtk_widget_queue_draw(state->sdraw_area);
		gtk_widget_queue_draw(state->draw_area);
	}
}

static gboolean stack_clicked(GtkWidget *, GdkEvent *event, State *state){
	double y = ((GdkEventButton*)event)->y;
	// convert to stack index if applicable
	y -= 5; // top margin = 5px
	unsigned index = ((int)y / 18);
	if(index < state->rs_stack.size()){
		state->active_frame = -index;
		state->rs = state->rs_stack[-state->active_frame];
		gtk_widget_queue_draw(state->sdraw_area);
		gtk_widget_queue_draw(state->draw_area);
	}
	return false;
}

static gboolean board_double_clicked(GtkWidget *, GdkEvent *event, State *state){
	if(event->type == GDK_2BUTTON_PRESS){
		if(state->movement_frame == -1 && !state->autoplay){
			const Board *board = state->rs->bc->board;
			// determine board x/y
			double mx = ((GdkEventButton*)event)->x, my = ((GdkEventButton*)event)->y;
			// board 0,0 top left coordinate location
			int w = board->width * 48, h = board->height * 48;
			int offx = (state->draw_area_width - w)/2, offy = (state->draw_area_height - h)/2;

			int bx = (mx - offx) / 48, by = (my - offy) / 48;
			if(bx >= 0 && bx < board->width && by >= 0 && by < board->height){
				// valid tile
				int index = board->index(bx, by);
				uint16_t &value = state->rs->cur_marbles[index];
				if(value & 0xFF00){
					// marble already present
					int tmp = prompt_marble_value(state, "Edit Marble", value & 0x00FF);
					if(tmp >= 0)
						value = 0xFF00 | tmp;
				}else{
					// new marble
					int tmp = prompt_marble_value(state, "New Marble");
					if(tmp >= 0)
						value = 0xFF00 | tmp;
				}
			}
		}
	}

	return false;
}