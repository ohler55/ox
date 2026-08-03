#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: an encoding set through Ox.default_options could not be
# unset again for the rest of the process.
#
# The option is held twice, as the name in options.encoding and as the
# rb_encoding * in options.rb_enc, and the two halves are read by different
# code. set_def_opts() cleared only the name, so:
#
#   Ox.default_options = {encoding: 'UTF-8'}
#   Ox.default_options = {encoding: nil}
#
#   Ox.default_options[:encoding]  #=> nil          says it is gone
#   Ox.dump('a').encoding          #=> ASCII-8BIT   reads the name, agrees
#   Ox.parse_obj(xml).encoding     #=> UTF-8        reads rb_enc, does not
#
# Ox.load was never affected: it re-derives rb_enc from the String it is handed
# whenever the name is empty. Ox.parse_obj and Ox.parse take the struct as it
# stands, so they kept applying an encoding that had been asked to go away.
#
# This is also what the save-and-restore idiom the test suite uses depends on -
# @opts = Ox.default_options in setup, Ox.default_options = @opts in teardown -
# since a saved Hash carries the cleared encoding back as nil.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class DefaultEncodingTest < ::Test::Unit::TestCase
  BINARY_XML = '<?xml version="1.0"?><s>abc</s>'.b

  def setup
    @opts = Ox.default_options
    Ox.default_options = {mode: :object, encoding: nil}
  end

  def teardown
    Ox.default_options = @opts
  end

  # The four ways an encoding reaches a result. dump and load read the name,
  # parse_obj and parse read rb_enc, so a disagreement shows up as a split.
  def encodings
    {
      parse_obj: Ox.parse_obj(BINARY_XML).encoding,
      parse: Ox.parse('<r>abc</r>'.b).text.encoding,
      load: Ox.load(BINARY_XML).encoding,
      dump: Ox.dump('a').encoding
    }
  end

  def test_clearing_the_encoding_clears_it_everywhere
    Ox.default_options = {mode: :object, encoding: 'UTF-8'}
    assert_equal({parse_obj: Encoding::UTF_8, parse: Encoding::UTF_8,
                  load: Encoding::UTF_8, dump: Encoding::UTF_8},
                 encodings)

    Ox.default_options = {mode: :object, encoding: nil}
    assert_nil(Ox.default_options[:encoding])
    assert_equal({parse_obj: Encoding::ASCII_8BIT, parse: Encoding::ASCII_8BIT,
                  load: Encoding::ASCII_8BIT, dump: Encoding::ASCII_8BIT},
                 encodings)
  end

  # Ox.default_options= replaces the set, so a hash that does not mention
  # :encoding clears it as surely as one that says nil.
  def test_an_option_hash_without_encoding_clears_it_too
    Ox.default_options = {mode: :object, encoding: 'UTF-8'}
    Ox.default_options = {mode: :object}
    assert_nil(Ox.default_options[:encoding])
    assert_equal([Encoding::ASCII_8BIT] * 4, encodings.values)
  end

  # What Ox.default_options reports and what a parse actually does have to be
  # the same answer, whichever half of the option each one is reading.
  def test_what_is_reported_matches_what_is_applied
    ['UTF-8', nil, 'UTF-8', 'US-ASCII', nil].each do |enc|
      Ox.default_options = {mode: :object, encoding: enc}
      expected = enc ? Encoding.find(enc) : Encoding::ASCII_8BIT
      assert_equal(enc, Ox.default_options[:encoding], "reported for #{enc.inspect}")
      encodings.each do |where, actual|
        assert_equal(expected, actual, "#{where} for #{enc.inspect}")
      end
    end
  end

  # A second encoding has to replace the first rather than be shadowed by it.
  def test_switching_between_encodings
    Ox.default_options = {mode: :object, encoding: 'UTF-8'}
    Ox.default_options = {mode: :object, encoding: 'US-ASCII'}
    assert_equal([Encoding::US_ASCII] * 4, encodings.values)
  end

  # The idiom every test file in this suite uses. On the unfixed code the
  # restore left rb_enc holding whatever the body had set, which is how this
  # defect was found - a test that passed alone and failed in company.
  def test_save_and_restore_round_trips
    saved = Ox.default_options
    Ox.default_options = {mode: :object, encoding: 'UTF-8'}
    Ox.default_options = saved
    assert_equal([Encoding::ASCII_8BIT] * 4, encodings.values)
  end

  # An encoding in the document is read per parse and has nothing to do with
  # the default, so clearing the default must not reach it.
  def test_a_document_encoding_still_wins
    Ox.default_options = {mode: :object, encoding: 'UTF-8'}
    Ox.default_options = {mode: :object, encoding: nil}
    doc = '<?xml version="1.0" encoding="UTF-8"?><s>abc</s>'.b
    assert_equal(Encoding::UTF_8, Ox.parse_obj(doc).encoding)
  end
end
