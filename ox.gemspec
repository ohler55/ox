
require 'date'
require File.join(File.dirname(__FILE__), 'lib/ox/version')

Gem::Specification.new do |s|
  s.name = "ox"
  s.version = ::Ox::VERSION
  s.authors = "Peter Ohler"
  s.date = Date.today.to_s
  s.email = "peter@ohler.com"
  s.homepage = "https://github.com/ohler55/ox"
  s.summary = "A fast XML parser and object serializer."
  s.description = "A fast XML parser and object serializer that uses only standard C lib."

  s.files = Dir["{lib,ext,test}/**/*.{rb,h,c,graffle}"] + ['LICENSE', 'README.rdoc']

  s.extensions = ["ext/ox/extconf.rb"]
  # s.executables = []

  s.require_paths = ["lib", "ext"]

  s.has_rdoc = true
  s.extra_rdoc_files = ['README.md']
  s.rdoc_options = ['--main', 'README.md']
  
  s.rubyforge_project = 'ox' # dummy to keep the build quiet
end
