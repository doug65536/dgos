#pragma once
#include "types.h"

void progress_bar_draw(int left, int top, int right, int percent,
                       const tchar *title, bool bar_only = false);
