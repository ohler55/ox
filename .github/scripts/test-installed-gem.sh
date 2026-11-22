#!/bin/sh
# Installs a precompiled Ox gem and runs the test suite against it.
#
# Usage: .github/scripts/test-installed-gem.sh PLATFORM GEM_DIR
#
# GEM_DIR holds the source gem and the PLATFORM gem, of one version. Ox is
# installed by name and version, as a user would, so this also checks that
# RubyGems picks the native gem over the source gem.
#
# Run from the repository root. It deletes lib/ and ext/ first so the tests
# cannot load the in-tree sources and must use the installed gem, so only run
# it in a throwaway checkout (CI).
set -eu

platform=$1
gem_dir=$2

rm -rf lib ext

# Install the version under test by name. Naming the version is what lets a
# prerelease such as 1.2.3.rc1 install at all; RubyGems skips prereleases when
# asked for just a name, even with --prerelease.
# shellcheck disable=SC2016 # Ruby code, not shell expansions
version=$(ruby -rrubygems/package -e '
  versions = Dir[File.join(ARGV[0], "ox-*.gem")].map { |g| Gem::Package.new(g).spec.version.to_s }.uniq
  abort "expected one version in #{ARGV[0]}, found #{versions}" unless versions.size == 1
  puts versions
' "$gem_dir")
(cd "$gem_dir" && gem install --local --no-document -v "$version" ox)

# Fail if RubyGems picked the source gem or would have compiled anything, and
# if Ox did not load the binary built for this Ruby's minor version. The
# version is pinned so another installed Ox can not stand in for this one.
# shellcheck disable=SC2016 # Ruby code, not shell expansions
ruby -e '
  gem "ox", ARGV[1]
  spec = Gem.loaded_specs.fetch("ox")
  abort "RubyGems picked the source gem over the #{ARGV[0]} gem" if spec.platform.to_s == "ruby"
  abort "the native gem still declares extensions: #{spec.extensions}" unless spec.extensions.empty?
  require "ox"
  loaded = $LOADED_FEATURES.grep(%r{/ox/\d+\.\d+/ox\.(so|bundle)\z})
  abort "did not load a precompiled binary" if loaded.empty?
  puts "ox #{Ox::VERSION} (#{spec.platform}) on ruby #{RUBY_VERSION}: #{loaded.first}"
' "$platform" "$version"

ruby test/tests.rb
ruby test/sax/sax_test.rb

# The same files rake test runs (see the Rakefile for why these are excluded).
tests=$(find test -name '*_test.rb' \
  ! -path test/cache_test.rb \
  ! -path test/cache8_test.rb \
  ! -path test/sax/smart_test.rb \
  ! -path test/sax/sax_test.rb | sort)
# shellcheck disable=SC2086 # split the file list into arguments
# ARGV is cleared so test-unit does not also treat the paths as its own options.
ruby -e 'files = ARGV.dup; ARGV.clear; files.each { |f| require File.expand_path(f) }' $tests
