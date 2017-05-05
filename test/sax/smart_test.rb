#!/usr/bin/env ruby
# encoding: UTF-8

# Ubuntu does not accept arguments to ruby when called using env. To get warnings to show up the -w options is
# required. That can be set in the RUBYOPT environment variable.
# export RUBYOPT=-w

$VERBOSE = true

$: << File.join(File.dirname(__FILE__), "../../lib")
$: << File.join(File.dirname(__FILE__), "../../ext")
$: << File.join(File.dirname(__FILE__), ".")

require 'stringio'
use_minitest = RUBY_VERSION.start_with?('2.1.') && RUBY_ENGINE != 'rbx'
if use_minitest
  require 'minitest/autorun'
else
  require 'test/unit'
end
require 'optparse'
require 'helpers'
require 'ox'


opts = OptionParser.new
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
opts.parse(ARGV)

test_case = (use_minitest) ? ::Minitest::Test : ::Test::Unit::TestCase

class SaxSmartTest < test_case
  include SaxTestHelpers

  NORMALELEMENTS = {
    "a" => {},
    "abbr" => {},
    "address" => {},
    "article" => {},
    "aside" => {},
    "audio" => {},
    "b" => {},
    "bdi" => {},
    "bdo" => {},
    "blockquote" => {},
    "body" => { "parents" => ["html"] },
    "button" => {},
    "canvas" => {},
    "caption" => { "parents" => ["table"] },
    "cite" => {},
    "colgroup" => { "parents" => ["table"], "childs" => ["col"] },
    "data" => {},
    "datalist" => { "childs" => ["option"]},
    "dl" => { "childs" => ["dt", "dd"] },
    "dt" => { "parents" => ["dl"] },
    "dd" => { "parents" => ["dl"] },
    "del" => {},
    "details" => { "childs" => ["summary"] },
    "dfn" => {},
    "div" => {},
    "em" => {},
    "fieldset" => {},
    "figcaption" => {},
    "figure" => {},
    "footer" => {},
    "form" => {},
    "h1" => {},
    "h2" => {},
    "h3" => {},
    "h4" => {},
    "h5" => {},
    "h6" => {},
    "head" => { "parents" => ["html"] },
    "hgroup" => { "childs" => ["h1", "h2", "h3", "h4", "h5", "h6"] },
    "html" => { "childs" => ["head", "body"] },
    "i" => {},
    "iframe" => {},
    "ins" => {},
    "kdb" => {},
    "label" => {},
    "legend" => { "parents" => ["fieldset"] },
    "li" => { "parents" => ["ul", "ol", "menu"] },
    "map" => {},
    "mark" => {},
    "menu" => { "childs" => ["li"] },
    "meter" => {},
    "nav" => {},
    "noscript" => {},
    "object" => { "childs" => ["param"] },
    "ol" => { "childs" => ["li"] },
    "optgroup" => { "parents" => ["select"], "childs" => ["option"] },
    "option" => { "parents" => ["optgroup", "select", "datalist"] },
    "output" => {},
    "p" => {},
    "pre" => {},
    "progress" => {},
    "q" => {},
    "rp" => { "parents" => ["ruby"] },
    "rt" => { "parents" => ["ruby"] },
    "ruby" => { "childs" => ["rt", "rp"] },
    "s" => {},
    "samp" => {},
    "script" => {},
    "section" => {},
    "select" => { "childs" => ["option", "optgroup"] },
    "small" => {},
    "span" => {},
    "strong" => {},
    "style" => {},
    "sub" => {},
    "summary" => { "parents" => ["details"] },
    "sup" => {},
    "table" => { "childs" => ["caption", "colgroup", "thead", "tbody", "tfoot", "tr"] },
    "tbody" => { "childs" => ["tr"], "parents" => ["table"] },
    "td" => { "parents" => ["tr"] },
    "textarea" => {},
    "tfoot" => { "parents" => ["table"], "childs" => ["tr"] },
    "th" => { "parents" => ["tr"] },
    "thead" => { "childs" => ["tr"], "parents" => ["table"] },
    "time" => {},
    "title" => { "parents" => ["head"] },
    "tr" => { "parents" => ["table", "thead", "tbody", "tfoot"], "childs" => ["td", "th"] },
    "u" => {},
    "ul" => { "childs" => ["li"] },
    "var" => {},
    "video" => {}
  }

  VOIDELEMENTS = {
    "area" => { "parents" => ["map"] },
    "base" => { "parents" => ["head"] },
    "br" => {},
    "col" => { "parents" => ["colgroup"] },
    "command" => { "parents" => ["colgroup"] },
    "embed" => {},
    "hr" => {},
    "img" => {},
    "input" => {},
    "keygen" => {},
    "link" => {},
    "meta" => { "parents" => ["head"] },
    "param" => { "parents" => ["object"] },
    "source" => { "parents" => ["audio", "video"] },
    "track" => { "parents" => ["audio", "video"] },
    "wbr" => {}
  }

  # Make the :smart => true option the default one
  def smart_parse_compare(xml, expected, handler = AllSax, opts = {}, handler_attr = :calls)
    parse_compare(xml, expected, handler, opts.merge(:smart => true, :skip => :skip_white), handler_attr)
  end

end

class ErrorsOnParentNormalElementTest < SaxSmartTest

  attr_reader :w

  def initialize(*args)
    @w = lambda { |s, ws| "<#{ws}>" + s + "</#{ws}>" }
    super(*args)
  end

  def parents_of(el)
    return [] if el == "html"
    NORMALELEMENTS[el]["parents"] || ["body"]
  end

  def ancestry_of(el, parent = nil)
    p = parent || parents_of(el)[0]
    [el] + (p ? ancestry_of(p) : [])
  end

  def ancestries(el)
    parents_of(el).map { |p| ancestry_of(el, p) }
  end

  def test_no_error_in_dependent_parent_elements
    NORMALELEMENTS.each_key do |el|
      ancestries(el).each do |arr|
        html = arr.reduce(&w)
        smart_parse_compare(html, [], ErrorSax, {}, :errors)
      end
    end
  end

end

class ErrorsOnParentVoidElementTest < SaxSmartTest

  def construct_html(el)
    s = ""
    str = (VOIDELEMENTS[el]["parents"] || []).each() do |p|
      s += "<#{p}><#{el}></#{p}>"
    end
    "<html><body>#{str}</body></html>"
  end

  def test_no_error_in_dependent_parent_on_void_elements
    VOIDELEMENTS.each_key do |el|
      smart_parse_compare(construct_html(el), [], ErrorSax, {}, :errors)
    end
  end

end

##
# A class that groups tests concerning DOCTYPE of an html document.
# More info: http://www.w3.org/TR/html5/syntax.html#the-doctype
##
class SaxSmartDoctypeTest < SaxSmartTest

  def test_doctype_html5
    html = %{<!DOCTYPE HTML>}
    smart_parse_compare(html, [[:doctype, " HTML"]])
  end

  def test_doctype_html401_strict
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">}
    smart_parse_compare(html, [[:doctype, " HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\""]])
  end

  def test_doctype_html401_transitional
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">}
    smart_parse_compare(html, [[:doctype, " HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\""]])
  end

  def test_doctype_html401_frameset
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Frameset//EN" "http://www.w3.org/TR/html4/frameset.dtd">}
    smart_parse_compare(html, [[:doctype, " HTML PUBLIC \"-//W3C//DTD HTML 4.01 Frameset//EN\" \"http://www.w3.org/TR/html4/frameset.dtd\""]])
  end

  def test_doctype_xhtml_10_transitional
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">}
    smart_parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\""]])
  end

  def test_doctype_xhtml_10_strict
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">}
    smart_parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\""]])
  end

  def test_doctype_xhtml_10_frameset
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Frameset//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd">}
    smart_parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.0 Frameset//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd\""]])
  end

  def test_doctype_xhtml_11
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">}
    smart_parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\""]])
  end

  def test_doctype_declaration_allcaps
    # allcaps is only required when not smart
    html = %{<!dOCTYPE HTML>}
    parse_compare(html,
                  [[:error, "Case Error: expected DOCTYPE all in caps", 1, 10],
                   [:doctype, " HTML"]], AllSax)
  end
end

##
# A class that groups tests concerning the normal elements of an html document.
# More info: http://www.w3.org/TR/html5/syntax.html#normal-elements
##
class SaxSmartNormalTagTest < SaxSmartTest

  def test_normal_tags
    html = %{<html class="class1">A terminated text</html>}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:attr, :class, "class1"],
                   [:text, "A terminated text"],
                   [:end_element, :html]])
  end

  def test_empty_element
    html = %{<html class="class1"><>A terminated text</html>}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:attr, :class, "class1"],
                   [:error, "Invalid Format: empty element", 1, 22],
                    [:text, "A terminated text"],
                   [:end_element, :html]])
  end

  def test_normal_self_closing_tags
    html = %{<br />}
    smart_parse_compare(html,
                  [[:start_element, :br],
                   [:end_element, :br]])
  end

  def test_with_no_close
    html = %{<html>}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:error, "Start End Mismatch: element 'html' not closed", 1, 6],
                   [:end_element, :html]])
  end

  def test_with_no_close_and_not_terminated_text
    html = %{<html>A not terminated text}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:error, "Not Terminated: text not terminated", 1, 27],
                   [:text, "A not terminated text"],
                   [:error, "Start End Mismatch: element 'html' not closed", 1, 27],
                   [:end_element, :html]])
  end

  def test_with_no_close_nested
    html = %{<html>Test <div style='width: 100px;'>A div element</html>}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:text, "Test "],
                   [:start_element, :div],
                   [:attr, :style, "width: 100px;"],
                   [:text, "A div element"],
                   [:error, "Start End Mismatch: element 'html' close does not match 'div' open", 1, 52],
                   [:end_element, :div],
                   [:end_element, :html]])
  end

  def test_with_no_close_with_nested
    html = %{<html><div class='test'>A div element</div>}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:start_element, :div],
                   [:attr, :class, "test"],
                   [:text, "A div element"],
                   [:end_element, :div],
                   [:error, "Start End Mismatch: element 'html' not closed", 1, 43],
                   [:end_element, :html]])
  end

  def test_with_no_close_and_not_terminated_text_with_nested
    html = %{<html>A not terminated <div>A div</div> text}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:text, "A not terminated "],
                   [:start_element, :div],
                   [:text, "A div"],
                   [:end_element, :div],
                   [:error, "Not Terminated: text not terminated", 1, 44],
                   [:text, " text"],
                   [:error, "Start End Mismatch: element 'html' not closed", 1, 44],
                   [:end_element, :html]])
  end

  def test_invalid_element_name
    html = %{
<html>
  <budy>Hello</budy>
</html>
}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:error, "Invalid Element: budy is not a valid element type for a HTML document type.", 3, 8],
                   [:start_element, :budy],
                   [:text, "Hello"],
                   [:end_element, :budy],
                   [:end_element, :html]])
  end

  def test_comment_active
    html = %{
<html>
  <body>
    <p>hello</p>
    <!-- a comment -->
  </body>
</html>
}
    overlay = Ox.sax_html_overlay.dup
    overlay.each_key {|k| overlay[k] = :off }
    overlay['!--'] = :active

    handler = AllSax.new()
    input = StringIO.new(html)
    options = {
      :overlay => overlay,
      :skip => :skip_white,
    }
    Ox.sax_html(handler, input, options)
    expected = [[:comment, " a comment "]]
    actual = handler.calls()

    puts "\nexpected: #{expected}\n  actual: #{actual}" if expected != actual
    assert_equal(expected, actual)
  end

  def test_nested
    html = %{
<html>
  <body>
    <div><div><div><div><div><div><div><div><div><div><div><div><div><div><div><div><div><div><div><div>
Word
    </div></div></div></div></div></div></div></div></div></div></div></div></div></div></div></div></div></div></div></div>
  </body>
</html>
}
    smart_parse_compare(html,
                  [[:start_element, :html], [:start_element, :body]] +
                  [[:start_element, :div]] * 20 +
                  [[:text, " Word "]] +
                  [[:end_element, :div]] * 20 +
                  [[:end_element, :body], [:end_element, :html]
                  ])
  end

  # The expected behaviour is to trigger an error and then trigger the start / end events
  # which is not the case for void elements, where only an error is triggered.
  def test_not_opened
    html = "</div>"
    smart_parse_compare(html,
                  [[:error, "Start End Mismatch: element 'div' closed but not opened", 1, 1],
                   [:start_element, :div],
                   [:end_element, :div]])
  end

  def test_not_opened_nested
    html = "<h1 class=a_class>A header</div> here</h1>"
    smart_parse_compare(html,
                  [[:start_element, :h1],
                   [:error, "Unexpected Character: attribute value not in quotes", 1, 11],
                   [:attr, :class, "a_class"],
                   [:text, "A header"],
                   [:error, "Start End Mismatch: element 'div' closed but not opened", 1, 27],
                   [:start_element, :div],
                   [:end_element, :div],
                   [:text, " here"],
                   [:end_element, :h1]])
  end

  def test_unquoted_slash
    html = "<a href=abc/>def</a>"
    smart_parse_compare(html,
                  [[:start_element, :a],
                   [:error, "Unexpected Character: attribute value not in quotes", 1, 9],
                   [:attr, :href, "abc/"],
                   [:text, "def"],
                   [:end_element, :a]])
  end

  def test_unquoted_slash_no_close
    html = "<a href=abc/>def"
    smart_parse_compare(html,
                  [[:start_element, :a],
                   [:error, "Unexpected Character: attribute value not in quotes", 1, 9],
                   [:attr, :href, "abc/"],
                   [:error, "Not Terminated: text not terminated", 1, 16],
                   [:text, "def"],
                   [:error, "Start End Mismatch: element 'a' not closed", 1, 16],
                   [:end_element, :a]])
  end
end

##
# A class that groups tests concerning the so called void elements of an html document.
# More info: http://www.w3.org/TR/html5/syntax.html#void-elements
##
=begin
class SaxSmartVoidTagTest < SaxSmartTest

  VOIDELEMENTS = [
                  #"area", # only in map
                  #"base", # only in head
                  "br",
                  #"col", # only in colgroup
                  #"command", # not a void tag
                  "embed",
                  "hr",
                  "img",
                  "input",
                  "keygen",
                  #"link", # only in head
                  #"meta", # only in head
                  "param",
                  #"source", # only in media (audio, video)
                  #"track", # only in media (audio, video)
                  "wbr"
                 ]

  def test_closed
    VOIDELEMENTS.each do |el|
      html = %{<html>A terminated <#{el}/> text</html>}
      smart_parse_compare(html,
                    [
                     [:start_element, :html],
                     [:text, "A terminated "],
                     [:start_element, el.to_sym],
                     [:end_element, el.to_sym],
                     [:text, " text"],
                     [:end_element, :html]
                    ])
    end
  end

  def test_not_closed
    VOIDELEMENTS.each do |el|
      html = %{<html>A terminated <#{el}> text</html>}
      smart_parse_compare(html,
                    [
                     [:start_element, :html],
                     [:text, "A terminated "],
                     [:start_element, el.to_sym],
                     [:end_element, el.to_sym],
                     [:text, " text"],
                     [:end_element, :html]
                    ])
    end
  end

  # Fix this error to be supported
  def test_invalid_syntax
    VOIDELEMENTS.each do |el|
      html = %{<html>A terminated <#{el}>nice\n</#{el}> text</html>}
      smart_parse_compare(html,
                    [
                     [:start_element, :html],
                     [:text, "A terminated "],
                     [:start_element, el.to_sym],
                     [:end_element, el.to_sym],
                     [:text, "nice\n"],
                     [:error, "Start End Mismatch: element '#{el}' should not have a separate close element", 2, 1],
                     [:text, " text"],
                     [:end_element, :html]
                    ])
    end
  end

end
=end

##
# A class that groups tests concerning the table element. Because this element
# assumes a specific nested structure its testing must be made seperately from
# the normal tags testing.
##
class SaxSmartTableTagTest < SaxSmartTest

  def test_html_table
    html = %{
<html>
  <body>Table
    <table>
      <tr><td>one</td><td>two</td></tr>
      <tr><td>bad<td>bad2</tr>
    </table>
  </body>
</html>
}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:start_element, :body],
                   [:text, "Table "],
                   [:start_element, :table],
                   [:start_element, :tr],
                   [:start_element, :td],
                   [:text, "one"],
                   [:end_element, :td],
                   [:start_element, :td],
                   [:text, "two"],
                   [:end_element, :td],
                   [:end_element, :tr],
                   [:start_element, :tr],
                   [:start_element, :td],
                   [:text, "bad"],
                   [:error, "Invalid Element: td can not be nested in a HTML document, closing previous.", 6, 21],
                   [:end_element, :td],
                   [:start_element, :td],
                   [:text, "bad2"],
                   [:error, "Start End Mismatch: element 'tr' close does not match 'td' open", 6, 26],
                   [:end_element, :td],
                   [:end_element, :tr],
                   [:end_element, :table],
                   [:end_element, :body],
                   [:end_element, :html]])
  end

  def test_html_bad_table
    html = %{
<html>
  <body>
    <table>
      <div>
        <tr><td>one</td></tr>
      </div>
    </table>
  </body>
</html>
}
    smart_parse_compare(html,
                  [[:start_element, :html],
                   [:start_element, :body],
                   [:start_element, :table],
                   [:start_element, :div],
                   [:error, "Invalid Element: tr can not be a child of a div in a HTML document.", 6, 12],
                   [:start_element, :tr],
                   [:start_element, :td],
                   [:text, "one"],
                   [:end_element, :td],
                   [:end_element, :tr],
                   [:end_element, :div],
                   [:end_element, :table],
                   [:end_element, :body],
                   [:end_element, :html]])
  end

  def html_parse_compare(xml, expected, opts = {})
    handler = AllSax.new()
    input = StringIO.new(xml)
    options = {
      :smart => true,
      :symbolize => true,
      :smart => false,
      :skip => :skip_white,
    }.merge(opts)

    Ox.sax_html(handler, input, options)

    actual = handler.send(:calls)

    puts "\nexpected: #{expected}\n  actual: #{actual}" if expected != actual
    assert_equal(expected, actual)
  end

  def test_nest_ok
    html = %{
<html>
  <body>Table
    <table>
      <tr><td>one<td>two</td></td></tr>
    </table>
  </body>
</html>
}

    hints = Ox.sax_html_overlay()
    hints["td"] = :nest_ok
    html_parse_compare(html,
                       [[:start_element, :html],
                        [:start_element, :body],
                        [:text, "Table "],
                        [:start_element, :table],
                        [:start_element, :tr],
                        [:start_element, :td],
                        [:text, "one"],
                        [:start_element, :td],
                        [:text, "two"],
                        [:end_element, :td],
                        [:end_element, :td],
                        [:end_element, :tr],
                        [:end_element, :table],
                        [:end_element, :body],
                        [:end_element, :html]],
                       {:overlay => hints})
  end
  
end
