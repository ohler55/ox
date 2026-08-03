#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a loaded Regexp ignored the document's encoding.
#
# parse_regexp() called rb_reg_new(), which takes bytes and so can not be told
# an encoding. The source came back ASCII-8BIT no matter what the document
# said, which is not a cosmetic difference:
#
#   * /\p{Hiragana}+/ raised RegexpError, because a binary regexp has no
#     Unicode properties to look the name up in
#   * /あ/ loaded as /\xE3\x81\x82/ -- a different Regexp, and == says so
#   * even an ASCII only source came back ASCII-8BIT where Ruby gives US-ASCII
#
# Regexp was the only value type doing this. String and Symbol have always
# taken pi->options->rb_enc, which is set from <?xml encoding?>, from
# Ox.default_options[:encoding], or from the encoding of the String handed to
# Ox.load. Going through rb_reg_new_str() also applies Ruby's own rule, which
# promotes an ASCII only source to US-ASCII the way a literal is written.
#
# Nothing changes when the document carries no encoding at all: the source is
# still ASCII-8BIT, which is exactly what a String does in the same document.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class RegexpEncodingTest < ::Test::Unit::TestCase
  # tests.rb leaves mode: :object behind, and :encoding is what this file is
  # about, so both are established here rather than saved and restored.
  BASE_OPTIONS = {mode: :object, encoding: nil}.freeze

  def setup
    @opts = Ox.default_options
    Ox.default_options = BASE_OPTIONS
  end

  def teardown
    Ox.default_options = @opts
  end

  ASCII = [//, /abc/, /a.c/i, /a/mix, /\A[[:alpha:]]+\z/].freeze
  WIDE = [/あ/, /\p{Hiragana}+/, /héllo/i, /日本語|한국어/].freeze

  def utf8_doc(re)
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>#{Ox.dump(re)}"
  end

  # The gate. On develop every one of these raises RegexpError or comes back as
  # a different Regexp.
  def test_a_declared_encoding_reaches_the_regexp
    WIDE.each do |re|
      back = Ox.parse_obj(utf8_doc(re))
      assert_instance_of(Regexp, back, re.inspect)
      assert_equal(Encoding::UTF_8, back.source.encoding, re.inspect)
      assert_equal(re, back, re.inspect)
    end
  end

  def test_the_default_encoding_option_reaches_it_too
    Ox.default_options = BASE_OPTIONS.merge(encoding: 'UTF-8')
    WIDE.each { |re| assert_equal(re, Ox.parse_obj(Ox.dump(re)), re.inspect) }
  end

  # Ox.load takes rb_enc from the String it is handed when nothing else says,
  # which is the third way in. Ox.parse_obj does not look at its argument's
  # encoding at all, so it is not the entry point to use here.
  def test_the_input_strings_encoding_reaches_it_too
    WIDE.each do |re|
      xml = Ox.dump(re).dup.force_encoding('UTF-8')
      assert_equal(re, Ox.load(xml, mode: :object), re.inspect)
    end
  end

  # An ASCII only source is US-ASCII, the way Regexp.new makes it, rather than
  # ASCII-8BIT. This is what made a loaded Regexp compare unequal to the same
  # literal on a Ruby whose Regexp#== looks at the source encoding.
  def test_an_ascii_source_is_promoted_to_us_ascii
    ASCII.each do |re|
      back = Ox.parse_obj(Ox.dump(re))
      assert_equal(Encoding::US_ASCII, back.source.encoding, re.inspect)
      assert_equal(re.source.encoding, back.source.encoding, re.inspect)
      assert_equal(re, back, re.inspect)
    end
  end

  def test_flags_survive_the_change
    assert_equal(Regexp::IGNORECASE, Ox.parse_obj('<g>/a/i</g>').options)
    assert_equal(Regexp::MULTILINE, Ox.parse_obj('<g>/a/m</g>').options)
    assert_equal(Regexp::EXTENDED, Ox.parse_obj('<g>/a/x</g>').options)
    assert_equal(//, Ox.parse_obj('<g>//</g>'))
  end

  # A pattern rb_reg_new_str() can not compile raises the same RegexpError
  # rb_reg_new() did, rather than becoming something else.
  def test_an_invalid_pattern_still_raises
    assert_raise(RegexpError) { Ox.parse_obj('<g>/(/</g>') }
    assert_raise(RegexpError) { Ox.parse_obj('<g>/[z-a]/</g>') }
  end

  def test_the_malformed_literal_check_is_unaffected
    assert_raise(Ox::ParseError) { Ox.parse_obj('<g>abc</g>') }
    assert_raise(Ox::ParseError) { Ox.parse_obj('<g>/</g>') }
  end

  # With no encoding anywhere the source stays ASCII-8BIT -- unchanged, and the
  # same thing a String does in that document. Pinned so that the boundary of
  # this change is written down rather than inferred.
  def test_without_any_encoding_a_regexp_matches_what_a_string_does
    from_string = Ox.parse_obj(Ox.dump('あ'))
    from_regexp = Ox.parse_obj(Ox.dump(/あ/))
    assert_equal(Encoding::ASCII_8BIT, from_string.encoding)
    assert_equal(Encoding::ASCII_8BIT, from_regexp.source.encoding)
  end

  # A Regexp inside a larger document takes the same encoding as the strings
  # beside it.
  def test_inside_a_document_with_other_values
    Ox.default_options = BASE_OPTIONS.merge(encoding: 'UTF-8')
    src = {a: /あ/, b: ['文字', /\p{Han}/i]}
    assert_equal(src, Ox.parse_obj(Ox.dump(src)))
  end
end
