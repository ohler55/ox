require 'mkmf'


headers = []
headers << "stdint.h" if have_header("stdint.h")

$CPPFLAGS += ' -Wall'
#puts "*** $CPPFLAGS: #{$CPPFLAGS}"
extension_name = 'ox'
dir_config(extension_name)

create_header()

create_makefile(extension_name)
