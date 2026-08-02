#!/usr/bin/env ruby
# frozen_string_literal: true

# Regression test: comment() tested the same overlay twice.
#
#   (NULL != h && (ActiveOverlay == h->overlay || ActiveOverlay == h->overlay))
#
# Every sibling test in sax.c -- end_element_cb, and the four sites around
# :895, :916, :1008 and :1260 -- is written "ActiveOverlay ... || NestOverlay
# ...", so the second term is a copy of the first where NestOverlay was meant.
#
# :off means "block this element and its children unless the child element is
# active", and :nest_ok means "active but ignore nest check". So a comment whose
# own hint is :nest_ok inside an :off element should be reported and was not.
# Nothing else reaches it: the default overlay for "!--" is :active, so both
# halves of the configuration have to be asked for.

$: << File.join(File.dirname(__FILE__), '../lib')
$: << File.join(File.dirname(__FILE__), '../ext')

require 'test/unit'
require 'ox'

class SaxCommentOverlayTest < ::Test::Unit::TestCase
  class Collect < ::Ox::Sax
    attr_reader :comments, :elements

    def initialize
      @comments = []
      @elements = []
    end

    def comment(value)
      @comments << value.strip
    end

    def start_element(name)
      @elements << name.to_s
    end

    def error(_message, _line, _column); end
  end

  HTML = '<html><body><div><!-- inside --></div><!-- outside --></body></html>'

  # Ox.sax_html_overlay returns the built-in table rather than whatever
  # Ox.default_options[:overlay] happens to hold, and passing every key back
  # makes the parse independent of a default another test file may have left
  # behind.
  def overlay(changes = {})
    Ox.sax_html_overlay.merge(changes)
  end

  def comments_for(over)
    handler = Collect.new
    Ox.sax_html(handler, HTML, overlay: over)
    handler.comments
  end

  # The gate. On develop this returns ["outside"] -- the comment inside the
  # :off div is dropped.
  def test_a_nest_ok_comment_inside_an_off_element_is_reported
    assert_equal(['inside', 'outside'], comments_for(overlay('div' => :off, '!--' => :nest_ok)))
  end

  # :nest_ok has to behave exactly like :active here, which is the whole claim.
  def test_nest_ok_matches_active
    assert_equal(comments_for(overlay('div' => :off, '!--' => :active)),
                 comments_for(overlay('div' => :off, '!--' => :nest_ok)))
  end

  # Everything below passes on develop too and must keep passing.

  def test_an_active_comment_inside_an_off_element_is_reported
    assert_equal(['inside', 'outside'], comments_for(overlay('div' => :off, '!--' => :active)))
  end

  def test_the_other_overlays_still_suppress_inside_an_off_element
    %i[inactive block off].each do |mode|
      assert_equal(['outside'], comments_for(overlay('div' => :off, '!--' => mode)), mode.to_s)
    end
  end

  # Without an :off parent the hint on "!--" does not gate anything, because the
  # first half of the condition has already let the comment through.
  def test_without_an_off_parent_every_overlay_reports_both
    %i[active nest_ok inactive block off].each do |mode|
      assert_equal(['inside', 'outside'], comments_for(overlay('!--' => mode)), mode.to_s)
    end
  end

  def test_the_default_overlay_reports_both
    assert_equal(['inside', 'outside'], comments_for(overlay))
    handler = Collect.new
    Ox.sax_html(handler, HTML)
    assert_equal(['inside', 'outside'], handler.comments)
  end

  # "!--" defaults to :active, so no configuration that leaves it alone can see
  # a difference from this change.
  def test_the_comment_hint_defaults_to_active
    assert_equal(:active, Ox.sax_html_overlay['!--'])
  end

  # An :off element still blocks its own callbacks; only the comment moved.
  def test_off_still_blocks_the_element_itself
    handler = Collect.new
    Ox.sax_html(handler, HTML, overlay: overlay('div' => :off, '!--' => :nest_ok))
    assert_equal(%w[html body], handler.elements)
  end
end
