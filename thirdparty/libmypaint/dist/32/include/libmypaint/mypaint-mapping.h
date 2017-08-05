#ifndef MAPPING_H
#define MAPPING_H

#include <mypaint-config.h>
#include <mypaint-glib-compat.h>

G_BEGIN_DECLS

typedef struct MyPaintMapping MyPaintMapping;

MyPaintMapping * mypaint_mapping_new(int inputs_);
void mypaint_mapping_free(MyPaintMapping *self);
float mypaint_mapping_get_base_value(MyPaintMapping *self);
void mypaint_mapping_set_base_value(MyPaintMapping *self, float value);
void mypaint_mapping_set_n (MyPaintMapping * self, int input, int n);
int mypaint_mapping_get_n (MyPaintMapping * self, int input);
void mypaint_mapping_set_point (MyPaintMapping * self, int input, int index, float x, float y);
void mypaint_mapping_get_point (MyPaintMapping * self, int input, int index, float *x, float *y);
gboolean mypaint_mapping_is_constant(MyPaintMapping * self);
int mypaint_mapping_get_inputs_used_n(MyPaintMapping *self);
float mypaint_mapping_calculate (MyPaintMapping * self, float * data);
float mypaint_mapping_calculate_single_input (MyPaintMapping * self, float input);


G_END_DECLS

#endif // MAPPING_H
