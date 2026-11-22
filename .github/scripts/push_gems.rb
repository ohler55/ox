# frozen_string_literal: true

# Pushes the gems in a directory to RubyGems.org, skipping any already there.
#
# Usage: ruby .github/scripts/push_gems.rb GEM_DIR [--dry-run]
#
# Expects one source gem plus native gems, all of one version. When run for a
# tag (GITHUB_REF_TYPE=tag) the tag must be v<version>, so a gem is never pushed
# under the wrong release. Native gems go first and the source gem last.
# Re-running is safe: a version and platform that RubyGems.org already has, for
# instance a source gem pushed by `rake release`, is skipped. Credentials come
# from the environment (GEM_HOST_API_KEY, set by
# rubygems/configure-rubygems-credentials).

require 'json'
require 'net/http'
require 'rubygems/package'

dir     = ARGV.fetch(0) { abort 'usage: push_gems.rb GEM_DIR [--dry-run]' }
dry_run = ARGV.include?('--dry-run')

specs = Dir[File.join(dir, '*.gem')].to_h { |path| [path, Gem::Package.new(path).spec] }
abort "no gems in #{dir}" if specs.empty?

names    = specs.values.map(&:name).uniq
versions = specs.values.map { |spec| spec.version.to_s }.uniq
abort "expected one gem name, found #{names}" unless names.size == 1
abort "expected one version, found #{versions}" unless versions.size == 1
name    = names.first
version = versions.first

if ENV['GITHUB_REF_TYPE'] == 'tag' && ENV['GITHUB_REF_NAME'] != "v#{version}"
  abort "tag #{ENV.fetch('GITHUB_REF_NAME', nil)} does not match gem version v#{version}"
end

source, native = specs.partition { |_, spec| spec.platform.to_s == 'ruby' }
abort "expected one source gem, found #{source.size}" unless source.size == 1

# Returns true if RubyGems.org has this version for this platform.
def published?(name, version, platform)
  uri = URI("https://rubygems.org/api/v2/rubygems/#{name}/versions/#{version}.json")
  uri.query = URI.encode_www_form(platform: platform) unless platform == 'ruby'
  response = Net::HTTP.get_response(uri)
  case response
  when Net::HTTPSuccess then true
  when Net::HTTPNotFound then false
  else abort "#{uri} returned #{response.code}"
  end
end

pushed = 0
(native.sort_by { |_, spec| spec.platform.to_s } + source).each do |path, spec|
  platform = spec.platform.to_s
  if published?(name, version, platform)
    puts "skip  #{File.basename(path)} (already on RubyGems.org)"
  elsif dry_run
    puts "would push #{File.basename(path)}"
  else
    puts "push  #{File.basename(path)}"
    system('gem', 'push', path, exception: true)
    pushed += 1
  end
end
puts dry_run ? 'dry run: nothing pushed' : "pushed #{pushed} gem(s)"
