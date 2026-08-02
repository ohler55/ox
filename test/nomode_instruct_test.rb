#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: a document must not be able to put a default Ox.load into
# object mode.
#
# With no :mode option and no Ox.default_options[:mode] the parse runs the
# NoMode callbacks, and nomode_instruct() used to honour every mode an
# <?ox mode="..."?> processing instruction named -- including "object", which
# swapped in the object callbacks mid-parse. Object mode allocates the classes
# the document names without calling initialize and sets their instance
# variables, so any Ox.load of untrusted XML reached that.
#
# The document may still pick :generic or :limited, since neither builds
# anything the document names. Only the caller can pick :object.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'tempfile'

require 'ox'

class NoModeInstructTest < ::Test::Unit::TestCase
  class Marker
    attr_accessor :v

    def initialize
      raise 'initialize must not run'
    end
  end

  # What Ox.dump(..., with_instructions: true) writes for a Marker.
  OBJ_XML = %{<?ox version="1.0" mode="object" circular="no" xsd_date="no"?>\n} +
            %{<o c="NoModeInstructTest::Marker">\n  <i a="@v">1</i>\n</o>\n}

  # These tests are about what happens with no mode set, so they cannot inherit
  # whatever the ambient defaults happen to be. tests.rb leaves them at
  # mode: :object, and the memcheck lane loads every test file into one
  # process, so that is what these would see there.
  BASE_OPTIONS = {
    mode:              nil,
    effort:            :strict,
    indent:            2,
    with_xml:          false,
    with_instructions: false,
    circular:          false,
    xsd_date:          false,
    skip:              :skip_white
  }.freeze

  def setup
    @default_options = Ox.default_options
    Ox.default_options = BASE_OPTIONS
  end

  def teardown
    Ox.default_options = @default_options
  end

  def test_dump_still_writes_the_object_instruction
    # Nothing about what Ox writes changes; only what a bare load does with it.
    obj    = Marker.allocate
    obj.v  = 1
    assert_equal(OBJ_XML, Ox.dump(obj, mode: :object, with_instructions: true))
  end

  def test_instruction_does_not_select_object_mode
    doc = Ox.load(OBJ_XML)
    assert_equal(Ox::Element, doc.class)
    assert_equal('o', doc.name)
    assert_equal('NoModeInstructTest::Marker', doc[:c])
  end

  def test_instruction_does_not_select_object_mode_from_a_file
    file = Tempfile.new(['nomode_instruct', '.xml'])
    begin
      # binmode so the newlines are not expanded to CRLF on Windows.
      # load_file() sizes the read from fstat but opens with fopen(path, "r"),
      # so a CRLF file is read short and raises LoadError.
      file.binmode
      file.write(OBJ_XML)
      file.close
      assert_equal(Ox::Element, Ox.load_file(file.path).class)
    ensure
      file.unlink
    end
  end

  def test_tolerant_effort_does_not_reopen_it
    # :tolerant only relaxes what happens when a named class is missing.
    assert_equal(Ox::Element, Ox.load(OBJ_XML, effort: :tolerant).class)
  end

  def test_caller_can_still_select_object_mode
    obj = Ox.load(OBJ_XML, mode: :object)
    assert_equal(Marker, obj.class)
    assert_equal(1, obj.v)
  end

  def test_caller_can_still_select_object_mode_by_default_options
    Ox.default_options = {mode: :object}
    assert_equal(Marker, Ox.load(OBJ_XML).class)
  end

  # An <?ox mode="object"?> in a document loaded in an explicit mode was always
  # ignored -- only NoMode read it. Pinned so a future fix does not restore it
  # by way of the generic callbacks.
  def test_generic_mode_still_ignores_the_instruction
    assert_equal(Ox::Element, Ox.load(OBJ_XML, mode: :generic).class)
  end

  # The three modes a document may still pick, each with a visible difference:
  # generic keeps the <?pro?> instruction as a node, NoMode drops it, and
  # limited has no instruct or comment callback at all so it never builds the
  # Ox::Document the <?xml?> prolog would.
  DOC = %{<?xml version="1.0"?>%s<?pro cat="quick"?><a><!--c--><b>x</b></a>}

  def test_document_can_still_select_generic
    doc = Ox.load(format(DOC, %{<?ox version="1.0" mode="generic"?>}))
    assert_equal(Ox::Document, doc.class)
    assert_equal([Ox::Instruct, Ox::Element], doc.nodes.map(&:class))
  end

  def test_no_instruction_stays_in_nomode
    doc = Ox.load(format(DOC, ''))
    assert_equal(Ox::Document, doc.class)
    assert_equal([Ox::Element], doc.nodes.map(&:class))
  end

  def test_document_can_still_select_limited
    doc = Ox.load(format(DOC, %{<?ox version="1.0" mode="limited"?>}))
    assert_equal(Ox::Element, doc.class)
    assert_equal('a', doc.name)
    assert_equal([Ox::Element], doc.nodes.map(&:class))
  end

  def test_unknown_mode_still_raises
    assert_raise(Ox::SyntaxError) { Ox.load(%{<?ox version="1.0" mode="bogus"?><a/>}) }
  end

  def test_unsupported_version_still_raises
    assert_raise(Ox::SyntaxError) { Ox.load(%{<?ox version="10" mode="object"?><a/>}) }
  end
end
