#include "cell.h"
#include "surfaces.h"
#include <cmath>
#include <string>
#include <cairo.h>
#include <pango/pangocairo.h>
#include <pango/pangoft2.h>

#ifndef M_PI
	#define M_PI 3.14159265358979323846
#endif

static cairo_surface_t *create_text_surface(
	int swidth,
	int sheight,
	const char *text
)
{
	cairo_t *scr;
	cairo_surface_t *surf;
	PangoLayout *playout;
	PangoFontMap *pfontmap;
	PangoContext *pcontext;
	// PangoFontDescription *pfontdesc;
	
	surf  = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, swidth, sheight);
	scr = cairo_create(surf);
	playout = pango_cairo_create_layout(scr);
	pfontmap = pango_ft2_font_map_new();
	pcontext = pango_font_map_create_context(pfontmap);

	// pfontdesc = pango_font_description_from_string(font);
	// pango_font_description_set_absolute_size(pfontdesc, font_size);
	// pango_font_description_set_weight(pfontdesc, PANGO_WEIGHT_HEAVY);
	// pango_layout_set_font_description(playout, pfontdesc);
	// pango_font_map_load_font(pfontmap, pcontext, pfontdesc);
	// pango_font_description_free(pfontdesc);

	// pango_layout_set_text(playout, text, -1);
	pango_layout_set_markup(playout, text, -1);

	cairo_new_path(scr);
	cairo_move_to(scr, 0, 0);
	cairo_set_line_width(scr, 1.25);
	// cairo_set_source_rgb(scr, r, g, b);
	pango_cairo_update_layout(scr, playout);
	// pango_cairo_layout_path(scr, playout);
	pango_cairo_show_layout(scr, playout);
	// cairo_stroke_preserve(scr);

	g_object_unref(pcontext);
	g_object_unref(pfontmap);
	g_object_unref(playout);

	cairo_destroy(scr);

	return surf;
}

cairo_surface_t *create_devices_surface(){
	// table, without markup
	// // \\ @  &  =  >  <  +  -
	// ^  << >> ~~ ]] }  {  \/ /\    // not a multiline comment
	// !! ?   <  >  ?  +  -
	//  0  1  2  3  4  5  6  7  8
	//  9  A  B  C  D  E  F  G  H
	//  I  J  K  L  M  N  O  P  Q
	//  R  S  T  U  V  W  X  Y  Z
	const char *text = 
"<span font='Courier New 24' font_weight='heavy'>\
<span foreground='#1BBEE3'>// \\\\ </span>\
<span foreground='#00E52F'>@  </span>\
<span foreground='#968AFC'>&amp;  </span>\
<span foreground='#F9FE44'>=  &gt;  &lt;  </span>\
<span foreground='#FF4922'>+  -  </span>\n\
<span foreground='#FF006B'>^  &lt;&lt; &gt;&gt; ~~ </span>\
<span foreground='#E2FE4D'>]] }  {  </span>\
<span foreground='#FF891F'>\\/ </span>\
<span foreground='#83DCF5'>/\\ </span>\n\
<span foreground='#FF0049'>!! </span>\
<span foreground='#FF38D1'>?  </span>\
<span foreground='#E2FE4D'> &lt;  &gt; </span>\
<span foreground='#FF38D1'> ? </span>\
<span foreground='#FF4922'> +  - </span>\n\
<span foreground='#FFFFFF'>\
 0  1  2  3  4  5  6  7  8\n\
 9  A  B  C  D  E  F  G  H\n\
 I  J  K  L  M  N  O  P  Q\n\
 R  S  T  U  V  W  X  Y  Z\
</span></span>";
	return create_text_surface(512, 256, text);
}

cairo_surface_t *create_printables_surface(){
	const char *text = 
"<span font='Courier New 24' font_weight='heavy'><span foreground='white'>\
&#032;  &#033;  &#034;  &#035;  &#036;  &#037;  &#038;  &#039;  &#040;\n\
&#041;  &#042;  &#043;  &#044;  &#045;  &#046;  &#047;  &#048;  &#049;\n\
&#050;  &#051;  &#052;  &#053;  &#054;  &#055;  &#056;  &#057;  &#058;\n\
&#059;  &#060;  &#061;  &#062;  &#063;  &#064;  &#065;  &#066;  &#067;\n\
&#068;  &#069;  &#070;  &#071;  &#072;  &#073;  &#074;  &#075;  &#076;\n\
&#077;  &#078;  &#079;  &#080;  &#081;  &#082;  &#083;  &#084;  &#085;\n\
&#086;  &#087;  &#088;  &#089;  &#090;  &#091;  &#092;  &#093;  &#094;\n\
&#095;  &#096;  &#097;  &#098;  &#099;  &#100;  &#101;  &#102;  &#103;\n\
&#104;  &#105;  &#106;  &#107;  &#108;  &#109;  &#110;  &#111;  &#112;\n\
&#113;  &#114;  &#115;  &#116;  &#117;  &#118;  &#119;  &#120;  &#121;\n\
&#122;  &#123;  &#124;  &#125;  &#126;\n\
 &#032;  &#033;  &#034;  &#035;  &#036;  &#037;  &#038;  &#039;  &#040;\n\
 &#041;  &#042;  &#043;  &#044;  &#045;  &#046;  &#047;  &#048;  &#049;\n\
 &#050;  &#051;  &#052;  &#053;  &#054;  &#055;  &#056;  &#057;  &#058;\n\
 &#059;  &#060;  &#061;  &#062;  &#063;  &#064;  &#065;  &#066;  &#067;\n\
 &#068;  &#069;  &#070;  &#071;  &#072;  &#073;  &#074;  &#075;  &#076;\n\
 &#077;  &#078;  &#079;  &#080;  &#081;  &#082;  &#083;  &#084;  &#085;\n\
 &#086;  &#087;  &#088;  &#089;  &#090;  &#091;  &#092;  &#093;  &#094;\n\
 &#095;  &#096;  &#097;  &#098;  &#099;  &#100;  &#101;  &#102;  &#103;\n\
 &#104;  &#105;  &#106;  &#107;  &#108;  &#109;  &#110;  &#111;  &#112;\n\
 &#113;  &#114;  &#115;  &#116;  &#117;  &#118;  &#119;  &#120;  &#121;\n\
 &#122;  &#123;  &#124;  &#125;  &#126;\
</span>\n\
<span foreground='#BABAFF' font='Courier New 22' font_weight='heavy'>\
0  1  2  3  4  5  6  7  8\n\
9  A  B  C  D  E  F\n\
 0  1  2  3  4  5  6  7  8\n\
 9  A  B  C  D  E  F\n\
</span></span>";
	return create_text_surface(512, 1024, text);
}

cairo_surface_t *create_cn16_surface(){
	const char *text = 
"<span font='Courier New 16' font_weight='heavy'><span foreground='white'>\
&#032;&#033;&#034;&#035;&#036;&#037;&#038;&#039;&#040;\n\
&#041;&#042;&#043;&#044;&#045;&#046;&#047;&#048;&#049;\n\
&#050;&#051;&#052;&#053;&#054;&#055;&#056;&#057;&#058;\n\
&#059;&#060;&#061;&#062;&#063;&#064;&#065;&#066;&#067;\n\
&#068;&#069;&#070;&#071;&#072;&#073;&#074;&#075;&#076;\n\
&#077;&#078;&#079;&#080;&#081;&#082;&#083;&#084;&#085;\n\
&#086;&#087;&#088;&#089;&#090;&#091;&#092;&#093;&#094;\n\
&#095;&#096;&#097;&#098;&#099;&#100;&#101;&#102;&#103;\n\
&#104;&#105;&#106;&#107;&#108;&#109;&#110;&#111;&#112;\n\
&#113;&#114;&#115;&#116;&#117;&#118;&#119;&#120;&#121;\n\
&#122;&#123;&#124;&#125;&#126;\
</span></span>";
	return create_text_surface(128, 256, text);
}

cairo_surface_t *create_marble_surface(){
	cairo_t *scr;
	cairo_surface_t *surf;
	cairo_pattern_t *pat;

	surf  = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 48, 48);
	scr = cairo_create(surf);
	pat = cairo_pattern_create_radial(40, 0, 10, 26, 20, 34.415);
	cairo_pattern_add_color_stop_rgba(pat, 1, 0.2, 0.0, 0.2, 1);
	cairo_pattern_add_color_stop_rgba(pat, 0, 0.8, 0.5, 0.8, 1);

	cairo_arc(scr, 24, 24, 22, 0, 2 * M_PI);
	cairo_clip(scr);
	cairo_new_path(scr);
	cairo_rectangle(scr, 0, 0, 48, 48);
	cairo_set_source(scr, pat);
	cairo_fill(scr);

	cairo_set_source_rgb(scr, 1.0, 0.0, 1.0);
	cairo_stroke_preserve(scr);

	cairo_pattern_destroy(pat);
	cairo_destroy(scr);
	return surf;
}

void draw_device(cairo_t *cr, cairo_surface_t *surf, Cell c, double x, double y){
	int i = -1, ei = -1; 
	// ei: which cell for second rectangle, i.e. second char if varying
	// see text variable in createDeviceSurface
	// base36 digits start on line 4/index 27 (9 per row)
	switch(c.device){
		case DV_LEFT_DEFLECTOR: i = 0; break;
		case DV_RIGHT_DEFLECTOR: i = 1; break;
		case DV_PORTAL: i = 2, ei = c.value + 27; break;
		case DV_SYNCHRONISER: i = 3, ei = c.value + 27; break;
		case DV_EQUALS: i = 4, ei = c.value + 27; break;
		case DV_GREATER_THAN: i = 5, ei = c.value + 27; break;
		case DV_LESS_THAN: i = 6, ei = c.value + 27; break;
		case DV_ADDER: i = 7, ei = c.value + 27; break;
		case DV_SUBTRACTOR: i = 8, ei = c.value + 27; break;
		case DV_INCREMENTOR: i = 7, ei = 23; break;
		case DV_DECREMENTOR: i = 8, ei = 24; break;
		case DV_BIT_CHECKER: i = 9, ei = c.value + 27; break;
		case DV_LEFT_BIT_SHIFTER: i = 10; break;
		case DV_RIGHT_BIT_SHIFTER: i = 11; break;
		case DV_BINARY_NOT: i = 12; break;
		case DV_STDIN: i = 13; break;
		case DV_INPUT: i = 14, ei = c.value + 27; break;
		case DV_OUTPUT: 
			i = 15;
			switch(c.value){
				case 255: ei = 20; break; // left output
				case 254: ei = 21; break; // right output
				default: ei = c.value + 27; break;
			}
		break;
		case DV_TRASH_BIN: i = 16; break;
		case DV_CLONER: i = 17; break;
		case DV_TERMINATOR: i = 18; break;
		case DV_RANDOM: i = 19, ei = (c.value == 253 ? 22 : c.value + 27); break;
		default: break;
	}
	if(i != -1){
		int ix = 57 * (i % 9), iy = 36 * (i / 9);
		cairo_surface_t *ssurf = cairo_surface_create_for_rectangle(surf, ix, iy, 40, 36);
		cairo_set_source_surface(cr, ssurf, x + 5, y + 6);
		cairo_paint(cr);
		cairo_surface_destroy(ssurf);
	}
	if(ei != -1){
		int eix = 57 * (ei % 9), eiy = 36 * (ei / 9);
		cairo_surface_t *ssurf = cairo_surface_create_for_rectangle(surf, eix, eiy, 40, 36);
		cairo_set_source_surface(cr, ssurf, x + 5, y + 6);
		cairo_paint(cr);
		cairo_surface_destroy(ssurf);
	}
}

void draw_board_call_cell(cairo_t *cr, cairo_surface_t *surf, std::string text, double x, double y){
	// space starts at 0; second chars start at line 10 (cell 99)
	int i = ((32 <= text[0] && text[0] < 127) ? (text[0] - 32) : 0);
	int ei = ((32 <= text[1] && text[1] < 127) ? (text[1] - 32 + 99) : 0);
	int ix = 57 * (i % 9), iy = 36 * (i / 9);
	int eix = 57 * (ei % 9), eiy = 36 * (ei / 9);

	cairo_surface_t *ssurf = cairo_surface_create_for_rectangle(surf, ix, iy, 40, 36);
	cairo_set_source_surface(cr, ssurf, x + 5, y + 6);
	cairo_paint(cr);
	cairo_surface_destroy(ssurf);
	ssurf = cairo_surface_create_for_rectangle(surf, eix, eiy, 40, 36);
	cairo_set_source_surface(cr, ssurf, x + 5, y + 6);
	cairo_paint(cr);
	cairo_surface_destroy(ssurf);
}

void draw_marble(cairo_t *cr, cairo_surface_t *printables, cairo_surface_t *marble, uint8_t value, double x, double y){
	int i = value / 16, ei = value % 16;
	int ix = (i % 9) * 51, // displacement is smaller: smaller font
	    iy = 22 * 36 + (i / 9) * 33, // 22 * 36: first 22 rows are size 36, following rows are 33 (smaller font)
	    eix = (ei % 9) * 51,
	    eiy = 22 * 36 + (2 + ei / 9) * 33;
	cairo_new_path(cr);
	cairo_set_line_width(cr, 0);
	cairo_rectangle(cr, x, y, 48, 48);
	cairo_set_source_surface(cr, marble, x, y);
	cairo_paint(cr);

	cairo_surface_t *ssurf = cairo_surface_create_for_rectangle(printables, ix, iy, 40, 36);
	cairo_set_source_surface(cr, ssurf, x + 7, y + 8);
	cairo_paint(cr);
	cairo_surface_destroy(ssurf);
	ssurf = cairo_surface_create_for_rectangle(printables, eix, eiy, 40, 36);
	cairo_set_source_surface(cr, ssurf, x + 7, y + 8);
	cairo_paint(cr);
	cairo_surface_destroy(ssurf);
}

void draw_text_cn16(cairo_t *cr, cairo_surface_t *cn16, std::string text, double x, double y){
	for(const auto &c : text){
		int t = ((32 <= c && c < 127) ? (c - 32) : 0);
		int tx = 13 * (t % 9), ty = 23 * (t / 9);

		cairo_surface_t *ssurf = cairo_surface_create_for_rectangle(cn16, tx, ty + 3, 13, 16);
		cairo_set_source_surface(cr, ssurf, x, y);
		cairo_paint(cr);
		cairo_surface_destroy(ssurf);

		x += 13;
	}
}