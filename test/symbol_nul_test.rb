#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a Symbol used as a name or value lost its length on the way
# out, so an embedded NUL truncated it silently.
#
# rb_id2name() hands back a C string, so every call site took the length with
# strlen(). A Symbol may hold a NUL, and there the length stopped short. The
# truncation happened before any validation could see the byte, so the checks
# added for the String paths never fired and the shorter name was written as if
# the caller had asked for it:
#
#   Ox::Builder#element(:"a\0b")   wrote <a/>, where "a\0b" raises
#   Ox.dump(:"a\0b")              wrote <m>a</m>, which loads back as :a
#
# The object mode round trip is the part that bites: a Hash keyed by :"a\0b"
# came back keyed by :a, a different Symbol, with nothing raised anywhere.
#
# The fix is rb_sym2str() plus RSTRING_LEN, which keeps the length. Nothing here
# asks for new validation - once the length survives, the checks that were
# already there do the work, and a Symbol lands on the same answer as the String
# it prints as.

$LOAD_PATH << File.join(File.dirname(__FILE__), '../lib')
$LOAD_PATH << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class SymbolNulTest < ::Test::Unit::TestCase
  MSG = "'\\#x00' is not a valid XML character."

  # tests.rb leaves its own defaults behind and three of them change what the
  # assertions below see, so establish all three rather than inherit them.
  #
  # :invalid_replace matters most. Ox.dump only raises on an invalid character
  # while allow_invalid is No, and that is the startup state alone: assigning
  # default_options without the key turns allow_invalid on, and no value turns
  # it back off. Asking for '' instead pins the arm this file wants - the byte
  # is dropped and the rest of the value is kept - whatever ran before it.
  BASE_OPTIONS = {mode: :object, skip: :skip_white, invalid_replace: ''}.freeze

  def setup
    @opts = Ox.default_options
    Ox.default_options = BASE_OPTIONS
  end

  def teardown
    Ox.default_options = @opts
  end

  # Every writer that takes a name, as a pair of blocks: the Symbol form and the
  # String form of the same name. The point of the fix is that the two agree.
  PAIRS = {
    'Builder#element' => [
      ->(n) { Ox::Builder.new { |b| b.element(n.to_sym) } },
      ->(n) { Ox::Builder.new { |b| b.element(n) } }
    ],
    'Builder#void_element' => [
      ->(n) { Ox::Builder.new { |b| b.void_element(n.to_sym) } },
      ->(n) { Ox::Builder.new { |b| b.void_element(n) } }
    ],
    'Builder attribute name' => [
      ->(n) { Ox::Builder.new { |b| b.element('ok', n.to_sym => 'v') } },
      ->(n) { Ox::Builder.new { |b| b.element('ok', n => 'v') } }
    ],
    'Element attribute name' => [
      ->(n) { e = Ox::Element.new('r'); e[n.to_sym] = 'v'; Ox.dump(e) },
      ->(n) { e = Ox::Element.new('r'); e[n] = 'v'; Ox.dump(e) }
    ]
  }.freeze

  def outcome
    yield
  rescue Ox::SyntaxError => e
    e.message
  end

  def test_a_symbol_name_agrees_with_the_string_it_prints_as
    PAIRS.each do |where, (sym, str)|
      %W[a\0b \0 a\0 ab\0 a\0b\0c #{'a' * 20}\0b].each do |n|
        assert_equal(outcome { str.call(n) }, outcome { sym.call(n) }, "#{where} #{n.inspect}")
      end
    end
  end

  def test_every_name_writer_raises_on_a_symbol_with_a_nul
    PAIRS.each do |where, (sym, _)|
      e = assert_raise(Ox::SyntaxError, where) { sym.call("a\0b") }
      assert_equal(MSG, e.message, where)
    end
  end

  # The word loop reads eight bytes at a time and the tail loop takes what is
  # left, so walk the NUL across more than two words.
  def test_a_nul_is_found_at_any_offset_in_a_symbol
    24.times do |n|
      name = "#{'a' * n}\0#{'b' * 24}"
      assert_raise(Ox::SyntaxError, "offset #{n}") do
        Ox::Builder.new { |b| b.element(name.to_sym) }
      end
    end
  end

  # A name at or past struct _element's 64 byte inline buffer is strdup'd, which
  # is the path #449 and #458 were about. Reaching it with a Symbol needs the
  # length to be right first.
  def test_a_long_symbol_name_raises_too
    assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.element("#{'a' * 70}\0b".to_sym) } }
    assert_raise(Ox::SyntaxError) { Ox::Builder.new { |b| b.element("\0#{'x' * 200}".to_sym) } }
  end

  # What the defect cost in practice, in one line. Everything after the NUL was
  # dropped along with it, so the value came back as a different Symbol and
  # nothing said so. A String in the same place has always kept its tail.
  def test_ox_dump_keeps_what_follows_the_nul_in_a_symbol
    assert_equal("<m>ab</m>\n", Ox.dump(:"a\0b"))
    assert_equal("<s>ab</s>\n", Ox.dump("a\0b"))
    assert_equal("<h>\n  <m>ab</m>\n  <i>1</i>\n</h>\n", Ox.dump({:"a\0b" => 1}))
    assert_equal("<a>\n  <m>ab</m>\n</a>\n", Ox.dump([:"a\0b"]))
  end

  # An ivar holding such a Symbol reaches the same place by another route.
  class Holder
    def initialize(v)
      @v = v
    end
  end

  def test_a_symbol_held_in_an_object_keeps_its_tail
    assert_equal("<o c=\"SymbolNulTest::Holder\">\n  <m a=\"@v\">ab</m>\n</o>\n",
                 Ox.dump(Holder.new(:"a\0b")))
  end

  def test_an_instruct_attribute_name_raises
    i = Ox::Instruct.new('xml')
    i[:"a\0b"] = 'v'
    doc = Ox::Document.new
    doc << i
    assert_raise(Ox::SyntaxError) { Ox.dump(doc) }
  end

  # Struct members are written by index, not by name, so a NUL in a member name
  # never reaches the output. Pinned so the fix is not read as covering it.
  NulMember = Struct.new(:"a\0b", :ok)

  def test_a_struct_member_name_is_not_affected
    assert_equal("<u c=\"SymbolNulTest::NulMember\">\n  <i a=\"0\">1</i>\n  <i a=\"1\">2</i>\n</u>\n",
                 Ox.dump(NulMember.new(1, 2)))
  end

  # Symbols with no NUL have to be untouched, including the ones that take the
  # base64 arm in object mode and the ones long enough to leave the fast path.
  def test_symbols_without_a_nul_are_unchanged
    [:a, :abc, :CamelCase, :_x, :'a<b', :'a&b', :'a b', :日本語, :é,
     ('x' * 20).to_sym, ('y' * 100).to_sym].each do |sym|
      assert_equal(sym, Ox.parse_obj(Ox.dump(sym)), sym.inspect)
    end
  end

  # A tab, newline or carriage return in a Symbol reaches the output intact.
  # What happens on the way back in is the :skip option's business, not this
  # change's: the default :skip_white turns it into a space, so the round trip
  # is lossy there and exact under the other two. Both arms are set explicitly
  # because leaving it to whatever ran first is what made Z27's test flaky.
  def test_whitespace_in_a_symbol_reaches_the_output
    assert_equal("<m>a\nb</m>\n", Ox.dump(:"a\nb"))
    assert_equal("<m>a\tb</m>\n", Ox.dump(:"a\tb"))
    assert_equal("<s>a\nb</s>\n", Ox.dump("a\nb"))

    Ox.default_options = {mode: :object, skip: :skip_white}
    assert_equal(:"a b", Ox.parse_obj(Ox.dump(:"a\nb")))
    Ox.default_options = {mode: :object, skip: :skip_none}
    assert_equal(:"a\nb", Ox.parse_obj(Ox.dump(:"a\nb")))
  end

  def test_a_symbol_name_still_writes_the_same_document
    xml = Ox::Builder.new do |b|
      b.element(:top, :a => '1') do
        b.element(:mid) { b.text('hello') }
        b.void_element(:br)
      end
    end
    assert_equal(%(<top a="1">\n  <mid>hello</mid>\n  <br>\n</top>\n).b, xml)
  end

  def test_a_symbol_keyed_hash_still_round_trips
    h = {a: 1, b: 'two', 'c' => :three}
    assert_equal(h, Ox.parse_obj(Ox.dump(h)))
  end
end
