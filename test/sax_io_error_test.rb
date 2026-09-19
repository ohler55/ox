#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: exceptions raised by the input IO's read method were mangled.
#
# rescue_cb() in sax_buf.c is the rb_rescue handler for the read()/readpartial()
# calls. rb_rescue passes it the exception instance, but it handed that instance
# to rb_raise(), which expects a class, so Ruby raised a TypeError about the
# argument type and the original error was lost. On top of that the handler
# treated TypeError and EOFError as end of input, so a TypeError raised inside
# read() for any other reason ended the parse silently.
#
# A stream wrapper enforcing a size limit, or an IO failing mid-read, now
# reports through Ox instead of having to stash the error and re-raise after
# Ox.sax_parse returns.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'stringio'
require 'ox'

class SaxIoErrorTest < ::Test::Unit::TestCase
  class NullSax < ::Ox::Sax
    def start_element(_n); end
    def end_element(_n); end
    def text(_t); end
    def attr(_n, _v); end
    def error(_m, _l, _c); end
  end

  class SizeLimit < StandardError; end

  # An initialize that does not accept a message: rebuilding the exception with
  # Class.new(msg) would fail on it, cloning the instance does not.
  class NoArgError < StandardError
    def initialize
      super('no arg error')
    end
  end

  # Fails partway through the document rather than on the first read, so the
  # parser has consumed some input and line/column are meaningful.
  #
  # read() and readpartial() live in separate classes on purpose: sax_buf_init
  # picks the readpartial path for anything that responds to readpartial, so an
  # object carrying both never exercises read().
  class FailingIO
    def initialize(body, error)
      @io = StringIO.new(body)
      @error = error
      @reads = 0
    end

    def read(max)
      @reads += 1
      raise @error if @reads > 1

      @io.read(max)
    end
  end

  class PartialFailingIO < FailingIO
    def readpartial(max)
      read(max)
    end
  end

  # Bigger than the parser's 4096 byte buffer so a second read is needed.
  BODY = "<top>#{'x' * 10_000}</top>".freeze

  def parse(io)
    Ox.sax_parse(NullSax.new, io)
  end

  def test_custom_error_from_read_reaches_the_caller
    e = assert_raise(SizeLimit) do
      parse(FailingIO.new(BODY, SizeLimit.new('response too large')))
    end
    assert_match(/response too large/, e.message)
  end

  def test_the_message_carries_the_position
    e = assert_raise(SizeLimit) do
      parse(FailingIO.new(BODY, SizeLimit.new('response too large')))
    end
    assert_match(/at line \d+, column \d+/, e.message)
  end

  def test_custom_error_from_readpartial_reaches_the_caller
    e = assert_raise(SizeLimit) do
      parse(PartialFailingIO.new(BODY, SizeLimit.new('response too large')))
    end
    assert_match(/response too large/, e.message)
  end

  def test_error_class_with_a_zero_argument_initialize_survives
    e = assert_raise(NoArgError) { parse(FailingIO.new(BODY, NoArgError.new)) }
    assert_match(/no arg error/, e.message)
  end

  # TypeError used to be swallowed as end of input, so the parse just stopped.
  def test_type_error_from_read_is_no_longer_swallowed
    assert_raise(TypeError) do
      parse(FailingIO.new(BODY, TypeError.new('not a string')))
    end
  end

  def test_io_error_from_read_reaches_the_caller
    assert_raise(IOError) { parse(FailingIO.new(BODY, IOError.new('closed stream'))) }
  end

  # EOFError stays end of input: that is how readpartial signals it.
  def test_eof_error_still_ends_the_parse
    assert_nothing_raised do
      parse(PartialFailingIO.new(BODY, EOFError.new))
    end
  end

  def test_eof_error_from_read_still_ends_the_parse
    assert_nothing_raised { parse(FailingIO.new(BODY, EOFError.new)) }
  end

  # read returning nil is the other end of input signal, and it used to work
  # only because StringValuePtr raised a TypeError on nil.
  class NilAtEofIO
    def initialize(body)
      @body = body
      @sent = false
    end

    def read(_max)
      return nil if @sent

      @sent = true
      @body
    end
  end

  def test_nil_from_read_still_ends_the_parse
    handler = Collect.new
    Ox.sax_parse(handler, NilAtEofIO.new('<top>x</top>'))
    assert_equal(['top'], handler.seen)
  end

  # An IO that never fails still parses through both IO paths.
  class WorkingIO
    def initialize(body)
      @io = StringIO.new(body)
    end

    def read(max)
      @io.read(max)
    end
  end

  class WorkingPartialIO < WorkingIO
    def readpartial(max)
      raise EOFError if @io.eof?

      @io.readpartial(max)
    end
  end

  class Collect < ::Ox::Sax
    attr_reader :seen

    def initialize
      super
      @seen = []
    end

    def start_element(name)
      @seen << name.to_s
    end

    def error(_m, _l, _c); end
  end

  def test_a_complete_document_still_parses_through_read
    handler = Collect.new
    Ox.sax_parse(handler, WorkingIO.new(BODY.dup))
    assert_equal(['top'], handler.seen)
  end

  def test_a_complete_document_still_parses_through_readpartial
    handler = Collect.new
    Ox.sax_parse(handler, WorkingPartialIO.new(BODY.dup))
    assert_equal(['top'], handler.seen)
  end
end
