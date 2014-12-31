
require 'date'
require File.join(File.dirname(__FILE__), 'lib/ox/version')

Gem::Specification.new do |s|
  s.name = "ox"
  s.version = ::Ox::VERSION
  s.authors = "Peter Ohler"
  s.date = Date.today.to_s
  s.email = "peter@ohler.com"
  s.homepage = "http://www.ohler.com/ox"
  s.summary = "A fast XML parser and object serializer."
  s.description = %{A fast XML parser and object serializer that uses only standard C lib.
            
Optimized XML (Ox), as the name implies was written to provide speed optimized
XML handling. It was designed to be an alternative to Nokogiri and other Ruby
XML parsers for generic XML parsing and as an alternative to Marshal for Object
serialization. }

  s.licenses = ['MIT']
  s.files = Dir["{lib,ext}/**/*.{rb,h,c}"] + ['README.md']

  s.extensions = ["ext/ox/extconf.rb"]
  # s.executables = []

  s.require_paths = ["lib", "ext"]

  s.has_rdoc = true
  s.extra_rdoc_files = ['README.md']
  s.rdoc_options = ['--main', 'README.md']
  
  s.rubyforge_project = 'ox'
end
