require 'date'
require File.join(File.dirname(__FILE__), 'lib/ox/version')

Gem::Specification.new do |s|
  s.name = 'ox'
  s.version = Ox::VERSION
  s.authors = 'Peter Ohler'
  s.email = 'peter@ohler.com'
  s.homepage = 'http://www.ohler.com/ox'
  s.summary = 'A fast XML parser and object serializer.'
  s.description = %{A fast XML parser and object serializer that uses only standard C lib.

Optimized XML (Ox), as the name implies was written to provide speed optimized
XML handling. It was designed to be an alternative to Nokogiri and other Ruby
XML parsers for generic XML parsing and as an alternative to Marshal for Object
serialization. }
  s.metadata = {
    'bug_tracker_uri' => 'https://github.com/ohler55/ox/issues',
    'changelog_uri' => 'https://github.com/ohler55/ox/blob/master/CHANGELOG.md',
    'documentation_uri' => 'http://www.ohler.com/ox/doc/index.html',
    'homepage_uri' => 'http://www.ohler.com/ox/',
    'source_code_uri' => 'https://github.com/ohler55/ox',
    'rubygems_mfa_required' => 'true'
  }

  s.licenses = ['MIT']
  s.files = Dir['{lib,ext}/**/*.{rb,h,c}'] + ['LICENSE', 'README.md', 'CHANGELOG.md']

  s.extensions = ['ext/ox/extconf.rb']
  # s.executables = []

  s.require_paths = ['lib', 'ext']

  s.extra_rdoc_files = ['README.md', 'CHANGELOG.md']
  s.rdoc_options = ['--main', 'README.md', '--title', 'Ox', '--exclude', 'extconf.rb', 'lib', 'ext/ox', 'README.md']

  s.required_ruby_version = '>= 2.7.0'

  s.metadata['rubygems_mfa_required'] = 'true'

  s.add_dependency 'bigdecimal', '>= 3.0'
end
