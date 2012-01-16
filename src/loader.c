/*
 *  Copyright 2011 Sebastian Ventura
 *  This file is part of procs.
 *
 *  procs is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
*
 *  procs is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with procs.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "loader.h"


#include <unistd.h>
#include <stdio.h>

extern int flags;

void
restore_process(struct proc_process saved_process) {
	char filename[100];
	sprintf(filename,"/tmp/%s",saved_process.name);
	FILE* tmp = fopen(filename,"w+");
	write_process(saved_process,tmp,write_to_file_pointer);
	fclose(tmp);
	execl("bin/virtualizer","virtualizer",filename,(flags & PROC_FLAG_SAVE_FILE)?"1":"0",NULL);
}

