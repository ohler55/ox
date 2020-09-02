require 'mkmf'

extension_name = 'ox'
dir_config(extension_name)

parts = RUBY_DESCRIPTION.split(' ')
type = parts[0].downcase()
type = 'ree' if 'ruby' == type && RUBY_DESCRIPTION.include?('Ruby Enterprise Edition')
platform = RUBY_PLATFORM
version = RUBY_VERSION.split('.')
puts ">>>>> Creating Makefile for #{type} version #{RUBY_VERSION} on #{platform} <<<<<"

dflags = {
  'RUBY_TYPE' => type,
  (type.upcase + '_RUBY') => nil,
  'RUBY_VERSION' => RUBY_VERSION,
  'RUBY_VERSION_MAJOR' => version[0],
  'RUBY_VERSION_MINOR' => version[1],
  'RUBY_VERSION_MICRO' => version[2],

  # Not sure what else to look for.
  'UNIFY_FIXNUM_AND_BIGNUM' => ('ruby' == type && '2' == version[0] && '4' <= version[1]) ? 1 : 0,
}

dflags.each do |k,v|
  if v.nil?
    $CPPFLAGS += " -D#{k}"
  else
    $CPPFLAGS += " -D#{k}=#{v}"
  end
end
$CPPFLAGS += ' -Wall'
#puts "*** $CPPFLAGS: #{$CPPFLAGS}"
CONFIG['warnflags'].slice!(/ -Wsuggest-attribute=format/)
CONFIG['warnflags'].slice!(/ -Wdeclaration-after-statement/)
CONFIG['warnflags'].slice!(/ -Wmissing-noreturn/)

have_func('rb_time_timespec')
have_func('rb_enc_associate')
have_func('rb_enc_find')
have_func('rb_struct_alloc_noinit')
have_func('rb_obj_encoding')
have_func('rb_ivar_foreach')

have_header('ruby/st.h')
have_header('sys/uio.h')

have_struct_member('struct tm', 'tm_gmtoff')

create_makefile(extension_name)

%x{make clean}
