#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: Ox.sax_parse registered a GC root for every parse, and
# Ractors parsing at the same time corrupted the root list (ox #277).

$LOAD_PATH << File.join(File.dirname(__FILE__), '../../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../../ext')

require 'test/unit'
require 'stringio'
require 'ox'

class SaxRactorTest < ::Test::Unit::TestCase
  class Handler < Ox::Sax
    def start_element(name); end
    def attr(name, value); end
    def text(value); end
    def end_element(name); end
  end

  def test_parse_in_ractors
    omit('needs Ractor') unless defined?(Ractor)
    experimental = Warning[:experimental]
    Warning[:experimental] = false
    xml = '<a b="c"><d e="f">text</d><g/></a>'
    ractors = Array.new(4) do
      Ractor.new(xml) do |x|
        1000.times { Ox.sax_parse(Handler.new, StringIO.new(x)) }
        :ok
      end
    end
    assert_equal([:ok] * 4, ractors.map { |r| r.respond_to?(:value) ? r.value : r.take })
  ensure
    Warning[:experimental] = experimental unless experimental.nil?
  end
end
