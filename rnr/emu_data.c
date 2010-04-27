
#include "trace.h"
#include "dataset.h"
#include "emu_data.h"

static int EMU_DATA = -1;

emu_data_t *emu_data(trace_t *t)
{
	return get_data((dataset_t*)&t->data, EMU_DATA);
}

emu_data_t *add_emu_data(trace_t *t)
{
	if ( EMU_DATA == -1 )
		EMU_DATA = register_type(sizeof(emu_data_t));

	return add_data((dataset_t*)&t->data, EMU_DATA);
}

void del_emu_data(trace_t *t)
{
	del_data((dataset_t*)&t->data, EMU_DATA);
}

