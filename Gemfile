source 'https://rubygems.org'

gemspec

group :development do
  gem 'rake-compiler', '>= 1.2', '< 2.0'
  gem 'rubocop', '~> 1.47', require: false
  gem 'test-unit', '~> 3.0'
  gem 'ruby_memcheck', '~> 3.0' if RUBY_PLATFORM.include?('linux') && Gem::Version.new(RUBY_VERSION) >= Gem::Version.new('3.0')
end
