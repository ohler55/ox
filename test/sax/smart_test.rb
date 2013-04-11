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
require 'test/unit'
require 'optparse'
require 'helpers'
require 'ox'


opts = OptionParser.new
opts.on("-h", "--help", "Show this display")                { puts opts; Process.exit!(0) }
opts.parse(ARGV)



class SaxSmartTest < ::Test::Unit::TestCase
  include SaxTestHelpers

  # Make the :smart => true option the default one
  def parse_compare(xml, expected, handler=AllSax, opts={})
    super(xml, expected, handler, opts.merge(:smart => true))
  end

end


##
# A class that groups tests concerning DOCTYPE of an html document.
# More info: http://www.w3.org/TR/html5/syntax.html#the-doctype
##
class SaxSmartDoctypeTest < SaxSmartTest

  def test_doctype_html5
    html = %{<!DOCTYPE HTML>}
    parse_compare(html, [[:doctype, " HTML"]])
  end

  def test_doctype_html401_strict
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">}
    parse_compare(html, [[:doctype, " HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\""]])
  end

  def test_doctype_html401_transitional
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">}
    parse_compare(html, [[:doctype, " HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\""]])
  end

  def test_doctype_html401_frameset
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Frameset//EN" "http://www.w3.org/TR/html4/frameset.dtd">}
    parse_compare(html, [[:doctype, " HTML PUBLIC \"-//W3C//DTD HTML 4.01 Frameset//EN\" \"http://www.w3.org/TR/html4/frameset.dtd\""]])
  end

  def test_doctype_xhtml_10_transitional
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">}
    parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\""]])
  end

  def test_doctype_xhtml_10_strict
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">}
    parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\""]])
  end

  def test_doctype_xhtml_10_frameset
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Frameset//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd">}
    parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.0 Frameset//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd\""]])
  end

  def test_doctype_xhtml_11
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">}
    parse_compare(html, [[:doctype, " html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\""]])
  end

  def test_doctype_declaration_allcaps
    html = %{<!dOCTYPE HTML>}
    parse_compare(html,
      [
        [:error, "Case Error: expected DOCTYPE all in caps", 1, 10],
        [:doctype, " HTML"]
      ])
  end

end



##
# A class that groups tests concerning the normal elements of an html document.
# More info: http://www.w3.org/TR/html5/syntax.html#normal-elements
##
class SaxSmartNormalTagTest < SaxSmartTest

  def test_normal_tags
    html = %{<html class="class1">A terminated text</html>}
    parse_compare(html,
      [
        [:start_element, :html],
        [:attr, :class, "class1"],
        [:text, "A terminated text"],
        [:end_element, :html]
      ])
  end

  def test_normal_self_closing_tags
    html = %{<br />}
    parse_compare(html,
      [
        [:start_element, :br],
        [:end_element, :br]
      ])
  end

  def test_with_no_close
    html = %{<html>}
    parse_compare(html,
      [
        [:start_element, :html],
        [:error, "Start End Mismatch: element 'html' not closed", 1, 6],
        [:end_element, :html]
      ])
  end

  def test_with_no_close_and_not_terminated_text
    html = %{<html>A not terminated text}
    parse_compare(html,
      [
        [:start_element, :html],
        [:error, "Not Terminated: text not terminated", 1, 27],
        [:text, "A not terminated text"],
        [:error, "Start End Mismatch: element 'html' not closed", 1, 27],
        [:end_element, :html]
      ])
  end

  def test_with_no_close_nested
    html = %{<html>Test <div style='width: 100px;'>A div element</html>}
    parse_compare(html,
      [
        [:start_element, :html],
        [:text, "Test "],
        [:start_element, :div],
        [:attr, :style, "width: 100px;"],
        [:text, "A div element"],
        [:error, "Start End Mismatch: element 'html' close does not match 'div' open", 1, 51],
        [:end_element, :div],
        [:end_element, :html]
      ])
  end

  def test_with_no_close_with_nested
    html = %{<html><div class='test'>A div element</div>}
    parse_compare(html,
      [
        [:start_element, :html],
        [:start_element, :div],
        [:attr, :class, "test"],
        [:text, "A div element"],
        [:end_element, :div],
        [:error, "Start End Mismatch: element 'html' not closed", 1, 43],
        [:end_element, :html]
      ])
  end

  def test_with_no_close_and_not_terminated_text_with_nested
    html = %{<html>A not terminated <div>A div</div> text}
    parse_compare(html,
      [
        [:start_element, :html],
        [:text, "A not terminated "],
        [:start_element, :div],
        [:text, "A div"],
        [:end_element, :div],
        [:error, "Not Terminated: text not terminated", 1, 44],
        [:text, " text"],
        [:error, "Start End Mismatch: element 'html' not closed", 1, 44],
        [:end_element, :html]
      ])
  end

  def test_invalid_element_name
    html = %{
<html>
  <budy>Hello</budy>
</html>
}
    parse_compare(html,
      [
        [:start_element, :html],
        [:error, "Invalid Element: budy is not a valid element type for a HTML document type.", 3, 9],
        [:start_element, :budy],
        [:text, "Hello"],
        [:end_element, :budy],
        [:end_element, :html]
      ])
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
    parse_compare(html,
      [
        [:start_element, :html], [:start_element, :body]] +
        [[:start_element, :div]] * 20 +
        [[:text, "\nWord\n    "]] +
        [[:end_element, :div]] * 20 +
        [[:end_element, :body], [:end_element, :html]
      ])
  end

  # The expected behaviour is to trigger an error and then trigger the start / end events
  # which is not the case for void elements, where only an error is triggered.
  def test_not_opened
    html = "</div>"
    parse_compare(html,
      [
        [:error, "Start End Mismatch: element 'div' closed but not opened", 1, 0],
        [:start_element, :div],
        [:end_element, :div]
      ])
  end

  def test_not_opened_nested
    html = "<h1 class=a_class>A header</div> here</h1>"
    parse_compare(html,
      [
        [:start_element, :h1],
        [:error, "Unexpected Character: attribute value not in quotes", 1, 11],
        [:attr, :class, "a_class"],
        [:text, "A header"],
        [:error, "Start End Mismatch: element 'div' closed but not opened", 1, 26],
        [:start_element, :div],
        [:end_element, :div],
        [:text, " here"],
        [:end_element, :h1]
      ])
  end
end



##
# A class that groups tests concerning the so called void elements of an html document.
# More info: http://www.w3.org/TR/html5/syntax.html#void-elements
##
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
      parse_compare(html,
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
      parse_compare(html,
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
      parse_compare(html,
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
    parse_compare(html,
      [
        [:start_element, :html],
        [:start_element, :body],
        [:text, "Table\n    "],
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
        [:error, "Invalid Element: td can not be nested in a HTML document, closing previous.", 6, 22],
        [:end_element, :td],
        [:start_element, :td],
        [:text, "bad2"],
        [:error, "Start End Mismatch: element 'tr' close does not match 'td' open", 6, 26],
        [:end_element, :td],
        [:end_element, :tr],
        [:end_element, :table],
        [:end_element, :body],
        [:end_element, :html]
      ])
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
    parse_compare(html,
      [
        [:start_element, :html],
        [:start_element, :body],
        [:start_element, :table],
        [:start_element, :div],
        [:error, "Invalid Element: tr can not be a child of a div in a HTML document.", 6, 13],
        [:start_element, :tr],
        [:start_element, :td],
        [:text, "one"],
        [:end_element, :td],
        [:end_element, :tr],
        [:end_element, :div],
        [:end_element, :table],
        [:end_element, :body],
        [:end_element, :html]
      ])
  end
end

