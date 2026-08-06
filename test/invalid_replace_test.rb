#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: :invalid_replace did nothing unless :effort was moved off its
# default, and what :effort left behind could not be read back.
#
# dump_str_value() decided the invalid character case like this:
#
#   if (StrictEffort == out->opts->effort) { rb_raise(...); }   <- decided here
#   if (Yes == out->opts->allow_invalid) { ... }                <- :invalid_replace
#
# The raise was settled before :invalid_replace was looked at, and :effort starts
# as :strict, so on a fresh process the documented dump option did nothing at
# all. Reaching it meant setting :effort, which is documented as a load option -
# "effort to use when an undefined class is encountered". The coupling ran both
# ways: anyone who set effort: :tolerant so that object mode would accept an
# undefined class also, silently, turned off the character check on dump.
#
# :invalid_replace now governs that path on its own and :effort no longer reaches
# it. That needs a third state, since "no replacement was asked for" and "the
# empty string was asked for" have to differ - raise, and drop the byte:
#
#   false   raise Ox::SyntaxError            <- the default
#   nil     write it as a hex reference
#   ""      drop it
#   "..."   write the replacement instead
#
# The nil case had its own problem. A character reference has to resolve to a
# Char and #x0 is not one, so &#x0000; is not XML - and worse, reading it back
# ends the text at the NUL and loses the rest without an error:
#
#   Ox.dump("a\0b")               #=> "<s>a&#x0000;b</s>\n"
#   Ox.parse_obj(Ox.dump("a\0b")) #=> "a"          the rest is gone, no error
#
# so a NUL is now dropped rather than written. Every other C0 byte round trips
# within ox and is still written as a reference.
#
# See https://github.com/ohler55/ox/issues/464 for the decision.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class InvalidReplaceTest < ::Test::Unit::TestCase
  # The four states, keyed by the value :invalid_replace is given.
  STATES = {false => :raise, nil => :hex, '' => :drop, '?' => :replace}.freeze

  EFFORTS = %i[strict tolerant auto_define].freeze

  def setup
    # default_options returns every key it accepts, so this restores exactly.
    @saved = Ox.default_options
    # No :invalid_replace here on purpose - a freshly built options Hash does not
    # carry the key, and the default it falls back to is what most of this file
    # is about.
    Ox.default_options = {mode: :object, effort: :strict, skip: :skip_white}
  end

  def teardown
    Ox.default_options = @saved
  end

  def dump(str, **opts)
    Ox.dump(str, **opts)
  rescue Ox::SyntaxError => e
    e
  end

  # The default has to keep raising, or every caller who never set the option
  # would silently start writing documents with the byte dropped. An assignment
  # that leaves the key out used to turn allow_invalid on instead, so any
  # unrelated default_options= quietly started writing &#xNNNN;.
  def test_the_default_raises
    assert_equal(false, Ox.default_options[:invalid_replace])
    assert_raise(Ox::SyntaxError) { Ox.dump("a\x01b") }
    assert_raise(Ox::SyntaxError) { Ox.dump("a\0b") }
  end

  def test_each_state_does_what_it_says
    assert_instance_of(Ox::SyntaxError, dump("a\x01b", invalid_replace: false))
    assert_equal("<s>a&#x0001;b</s>\n", dump("a\x01b", invalid_replace: nil))
    assert_equal("<s>ab</s>\n", dump("a\x01b", invalid_replace: ''))
    assert_equal("<s>a?b</s>\n", dump("a\x01b", invalid_replace: '?'))
    assert_equal("<s>a[bad]b</s>\n", dump("a\x01b", invalid_replace: '[bad]'))
  end

  # The point of the change: the option works on a process that never touched
  # :effort, which is the state every process starts in.
  def test_the_option_works_without_touching_effort
    assert_equal(:strict, Ox.default_options[:effort])
    assert_equal("<s>a?b</s>\n", dump("a\x01b", invalid_replace: '?'))
  end

  # ...and :effort cannot reach the character check from any of its values.
  def test_effort_does_not_reach_the_character_check
    EFFORTS.each do |effort|
      STATES.each_key do |state|
        with = dump("a\x01b", effort: effort, invalid_replace: state)
        without = dump("a\x01b", invalid_replace: state)
        if with.is_a?(Ox::SyntaxError)
          assert_instance_of(Ox::SyntaxError, without, "#{effort} #{state.inspect}")
        else
          assert_equal(without, with, "#{effort} #{state.inspect}")
        end
      end
    end
  end

  # An :effort on its own no longer decides anything about characters, so it
  # falls through to the default, which raises.
  def test_effort_alone_raises_whatever_it_is_set_to
    EFFORTS.each do |effort|
      assert_raise(Ox::SyntaxError, effort.to_s) { Ox.dump("a\x01b", effort: effort) }
    end
  end

  def test_a_nul_is_dropped_rather_than_written
    assert_equal("<s>ab</s>\n", dump("a\0b", invalid_replace: nil))
    assert_equal("<s>ab</s>\n", dump("a\0b", invalid_replace: ''))
    assert_equal("<s>a?b</s>\n", dump("a\0b", invalid_replace: '?'))
    assert_instance_of(Ox::SyntaxError, dump("a\0b", invalid_replace: false))
  end

  # A NUL is the only byte allow_invalid refuses to write, because it is the
  # only one that costs the rest of the document on the way back.
  def test_every_other_c0_byte_is_still_written_as_a_reference
    (1..0x1f).each do |b|
      next if [0x09, 0x0a, 0x0d].include?(b)

      assert_equal(format('<s>a&#x%04x;b</s>%s', b, "\n"),
                   dump("a#{b.chr}b", invalid_replace: nil),
                   format('0x%02x', b))
    end
  end

  def test_the_tabs_and_newlines_xml_allows_are_untouched
    ["\t", "\n", "\r"].each do |c|
      STATES.each_key do |state|
        assert_equal("<s>a#{c}b</s>\n", dump("a#{c}b", invalid_replace: state), c.inspect)
      end
    end
  end

  # What allow_invalid writes now survives a round trip through ox, which is
  # what &#x0000; did not.
  def test_what_is_written_reads_back
    Ox.default_options = {mode: :object, skip: :skip_none, invalid_replace: nil}
    assert_equal('ab', Ox.parse_obj(Ox.dump("a\0b")))
    assert_equal("a\x01b", Ox.parse_obj(Ox.dump("a\x01b")))
    assert_equal("a\x1fb", Ox.parse_obj(Ox.dump("a\x1fb")))
  end

  def test_the_message_still_names_the_character
    e = dump("a\x01b", invalid_replace: false)
    assert_equal("'\\#x01' is not a valid XML character.", e.message)
    e = dump("a\0b", invalid_replace: false)
    assert_equal("'\\#x00' is not a valid XML character.", e.message)
  end

  # The byte is found wherever it sits: the escape scan reads a word at a time,
  # then a tail shorter than a word, then one flagged byte.
  def test_the_byte_is_found_at_any_offset
    24.times do |n|
      s = "#{'a' * n}\x01#{'b' * 24}"
      assert_raise(Ox::SyntaxError, "offset #{n} raise") { Ox.dump(s, invalid_replace: false) }
      assert_equal("<s>#{'a' * n}#{'b' * 24}</s>\n", dump(s, invalid_replace: ''), "offset #{n} drop")
    end
  end

  def test_more_than_one_bad_byte
    assert_equal("<s>a-b-c</s>\n", dump("a\x01b\x02c", invalid_replace: '-'))
    assert_equal("<s>abc</s>\n", dump("a\0b\0c", invalid_replace: nil))
    assert_equal("<s>--</s>\n", dump("\x01\x02", invalid_replace: '-'))
  end

  # An attribute value goes through the same escape path with a different table.
  def test_an_attribute_value_takes_the_option_too
    e = Ox::Element.new('r')
    e[:k] = "a\x01b"
    assert_raise(Ox::SyntaxError) { Ox.dump(e, mode: :generic, invalid_replace: false) }
    assert_equal(%(\n<r k="a?b"/>\n), Ox.dump(e, mode: :generic, invalid_replace: '?'))
    assert_equal(%(\n<r k="ab"/>\n), Ox.dump(e, mode: :generic, invalid_replace: ''))
    assert_equal(%(\n<r k="a&#x0001;b"/>\n), Ox.dump(e, mode: :generic, invalid_replace: nil))
  end

  def test_object_mode_values_take_the_option_too
    assert_equal("<m>a?b</m>\n", dump(:"a\x01b", invalid_replace: '?'))
    assert_equal("<h>\n  <s>a?b</s>\n  <i>1</i>\n</h>\n",
                 Ox.dump({"a\x01b" => 1}, invalid_replace: '?'))
  end

  # A name is written raw rather than escaped, so it is not this option's to
  # govern. It used to go out unchecked; since issue #469 it raises instead, and
  # the option does not soften that. Pinned so a change to either side shows up.
  def test_names_are_not_covered_by_the_option
    STATES.each_key do |state|
      assert_raise(Ox::SyntaxError, state.inspect) do
        Ox.dump(Ox::Element.new("r\x01"), mode: :generic, invalid_replace: state)
      end
      e = Ox::Element.new('r')
      e["k\x01"] = 'v'
      assert_raise(Ox::SyntaxError, state.inspect) { Ox.dump(e, mode: :generic, invalid_replace: state) }
    end
  end

  def test_a_call_option_beats_the_default
    Ox.default_options = {mode: :object, invalid_replace: '?'}
    assert_equal("<s>a?b</s>\n", Ox.dump("a\x01b"))
    assert_raise(Ox::SyntaxError) { Ox.dump("a\x01b", invalid_replace: false) }
    assert_equal("<s>ab</s>\n", Ox.dump("a\x01b", invalid_replace: ''))
  end

  def test_the_default_survives_a_call_that_does_not_mention_it
    Ox.default_options = {mode: :object, invalid_replace: '?'}
    Ox.dump("ok", effort: :tolerant)
    assert_equal("<s>a?b</s>\n", Ox.dump("a\x01b"))
    assert_equal('?', Ox.default_options[:invalid_replace])
  end

  # Every state has to come back as it went in, or a caller who saves and
  # restores default_options changes the behaviour by doing so.
  def test_default_options_round_trips_every_state
    STATES.each_key do |state|
      Ox.default_options = {mode: :object, invalid_replace: state}
      assert_equal(state, Ox.default_options[:invalid_replace], state.inspect)

      saved = Ox.default_options
      Ox.default_options = saved
      assert_equal(saved, Ox.default_options, state.inspect)
      assert_equal(state, Ox.default_options[:invalid_replace], state.inspect)
    end
  end

  # A key that is not there is the default, whatever else the assignment sets...
  def test_an_assignment_without_the_key_leaves_the_default
    Ox.default_options = {mode: :object, invalid_replace: '?'}
    Ox.default_options = {mode: :object}
    assert_equal(false, Ox.default_options[:invalid_replace])
    assert_raise(Ox::SyntaxError) { Ox.dump("a\x01b") }
  end

  # ...but an explicit nil still asks for the character to be written, and the
  # two have to stay distinguishable.
  def test_an_explicit_nil_is_not_a_missing_key
    Ox.default_options = {mode: :object, invalid_replace: nil}
    assert_nil(Ox.default_options[:invalid_replace])
    assert_equal("<s>a&#x0001;b</s>\n", Ox.dump("a\x01b"))
  end

  # The escape table reserves ten bytes for an invalid character, which is what
  # the longest replacement takes, so a value that is nothing but bad bytes is
  # where the reservation runs out first.
  def test_the_longest_replacement_on_every_byte
    s = "\x01" * 100
    assert_equal("<s>#{'x' * 10 * 100}</s>\n", dump(s, invalid_replace: 'x' * 10))
    assert_equal("<s>#{'&#x0001;' * 100}</s>\n", dump(s, invalid_replace: nil))
    assert_equal("<s></s>\n", dump(s, invalid_replace: ''))
    assert_equal("<s></s>\n", dump("\0" * 100, invalid_replace: nil))
  end

  def test_the_replacement_length_limit_is_unchanged
    assert_equal("<s>a#{'x' * 10}b</s>\n", dump("a\x01b", invalid_replace: 'x' * 10))
    assert_raise(Ox::ParseError) { Ox.dump("a\x01b", invalid_replace: 'x' * 11) }
    assert_raise(Ox::ParseError) { Ox.default_options = {invalid_replace: 'x' * 11} }
  end

  def test_a_value_that_is_neither_a_string_nor_false_still_raises
    assert_raise(TypeError) { Ox.dump("a\x01b", invalid_replace: 123) }
    assert_raise(TypeError) { Ox.default_options = {invalid_replace: 123} }
    assert_raise(TypeError) { Ox.default_options = {invalid_replace: :nope} }
  end

  # true is not one of the three, and reading it as "replace with nothing"
  # would be a guess, so it is refused like any other non-String.
  def test_true_is_not_a_state
    assert_raise(TypeError) { Ox.dump("a\x01b", invalid_replace: true) }
  end

  # Ox::Builder has its own check and has never taken this option. Pinned so
  # that bringing the two together is a deliberate change rather than a
  # side effect.
  def test_builder_is_unaffected
    STATES.each_key do |state|
      Ox.default_options = {mode: :object, invalid_replace: state}
      assert_raise(Ox::SyntaxError, state.inspect) do
        Ox::Builder.new(indent: -1) { |b| b.element('r') { b.text("a\x01b") } }
      end
    end
  end

  # Nothing above may change what a document with nothing wrong in it looks
  # like, whatever the option is set to.
  def test_valid_output_is_unchanged
    STATES.each_key do |state|
      xml = Ox.dump({'k' => 'hello & <world>', 'utf8' => '日本語 ☃ é'},
                    mode: :object, invalid_replace: state)
      # No :encoding was asked for, so what comes back is bytes.
      assert_equal("<h>\n  <s>k</s>\n  <s>hello &amp; &lt;world&gt;</s>\n" \
                   "  <s>utf8</s>\n  <s>日本語 ☃ é</s>\n</h>\n".b,
                   xml, state.inspect)
    end
  end
end
