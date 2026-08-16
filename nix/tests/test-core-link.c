#include <string.h>

#include <opentrackir/tir5.h>

int
main (void)
{
	const char *status = otir_status_string (OTIR_STATUS_OK);

	return status != NULL && strcmp (status, "ok") == 0 ? 0 : 1;
}
