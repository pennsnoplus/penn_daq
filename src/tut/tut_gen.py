# automatically makes defined commands in penn_daq.c
# tab-completable in the TUT 

import re

try:
  # open the penn_daq source
  penn_daq = open("src/net/net_utils.c", "r")
  penn_daq_lines = penn_daq.readlines()
  penn_daq.close()
  
  # find valid commands
  linemarker = 0
  valid_commands = []
  while linemarker < len(penn_daq_lines):
    line = penn_daq_lines[linemarker]
    if line.strip() == "//_!_begin_commands_!_":
      linemarker += 1
      while line.strip() != "//_!_end_commands_!_":
        line = penn_daq_lines[linemarker]
        if "if (strncmp(buffer," in line:
          valid_commands.append(re.findall(r'(?<=\").+(?=\")', line)[0])
        linemarker += 1
      break
    linemarker += 1
  
  # open the tut source
  tut = open("src/tut/tut.c", "r")
  tut_lines = tut.readlines()
  tut.close()
  
  # insert valid commands into a copy of the tut source
  linemarker = 0
  beginning = end = 0
  while linemarker < len(tut_lines):
    line = tut_lines[linemarker]
    if line.strip() == "//_!_begin_commands_!_":
      linemarker += 1
      beginning = linemarker
      while line.strip() != "//_!_end_commands_!_":
        linemarker += 1
        line = tut_lines[linemarker]
      end = linemarker
      break
    linemarker += 1
  
  if beginning == end == 0:
    raise ValueError
	
	
  out = tut_lines[:beginning]
  out.extend(["    { \"%s\", (Function *)NULL, (char *)NULL },\n" % com for com in valid_commands])
  out.extend(tut_lines[end:])
  
  # write the updated source to the original file
  tut2 = open("src/tut/tut.c", "w")
  tut2.write(''.join(out))
  tut2.close()
  
  print "tut_gen: updated tut.c with correct commands"
except:
  print "tut_gen: ERROR: unable to update tut.c"
