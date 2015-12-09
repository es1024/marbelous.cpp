#ifndef SURFACES_H
#define SURFACES_H

#include "cell.h"
#include <string>
#include <cairo.h>

cairo_surface_t *create_devices_surface();
cairo_surface_t *create_printables_surface();
cairo_surface_t *create_marble_surface();
cairo_surface_t *create_cn16_surface();
void draw_device(cairo_t *cr, cairo_surface_t *surf, Cell d, double x, double y);
void draw_board_call_cell(cairo_t *cr, cairo_surface_t *surf, std::string text, double x, double y);
void draw_marble(cairo_t *cr, cairo_surface_t *printables, cairo_surface_t *marble, uint8_t value, double x, double y);
void draw_text_cn16(cairo_t *cr, cairo_surface_t *cn16, std::string text, double x, double y);

#endif // SURFACES_H
