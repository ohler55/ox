#!/usr/bin/env ruby
# encoding: UTF-8

# Ubuntu does not accept arguments to ruby when called using env. To get warnings to show up the -w options is
# required. That can be set in the RUBYOPT environment variable.
# export RUBYOPT=-w

$VERBOSE = true

$: << File.join(File.dirname(__FILE__), "../../lib")
$: << File.join(File.dirname(__FILE__), "../../ext")
$: << File.join(File.dirname(__FILE__), "../sax")

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

class SaxSmartDoctypeTest < SaxSmartTest

  def test_doctype_html5
    html = %{<!DOCTYPE HTML>}
    parse_compare(html, [[:doctype, "HTML 5"]])
  end

  def test_doctype_html401_strict
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">}
    parse_compare(html, [[:doctype, "HTML 4.01"]])
  end

  def test_doctype_html401_transitional
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">}
    parse_compare(html, [[:doctype, "HTML 4.01 Transitional"]])
  end

  def test_doctype_html401_frameset
    html = %{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Frameset//EN" "http://www.w3.org/TR/html4/frameset.dtd">}
    parse_compare(html, [[:doctype, "HTML 4.01 FrameSet"]])
  end

  def test_doctype_xhtml_10_transitional
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">}
    parse_compare(html, [[:doctype, "XHTML 1.0 Transitional"]])
  end

  def test_doctype_xhtml_10_strict
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Strict//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd">}
    parse_compare(html, [[:doctype, "XHTML 1.0 Strict"]])
  end

  def test_doctype_xhtml_10_frameset
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Frameset//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-frameset.dtd">}
    parse_compare(html, [[:doctype, "XHTML 1.0 Frameset"]])
  end

  def test_doctype_xhtml_11
    html = %{<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">}
    parse_compare(html, [[:doctype, "XHTML 1.1"]])
  end

  def test_doctype_declaration_allcaps
    html = %{<!dOCTYPE HTML>}
    parse_compare(html,
      [
        [:error, "Case Error: expected DOCTYPE all in caps", 1, 10],
        [:doctype, "HTML 5"]
      ])
  end

end

class SaxSmartTagOpenTest < SaxSmartTest

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

  # NOTE: Not closed br tag will have to be closed immediately
  def test_nested_not_closed_br_tag
    html = %{<html>A terminated <br> text</html>}
    parse_compare(html,
      [
        [:start_element, :html],
        [:text, "A terminated "],
        [:start_element, :br],
        [:error, "Start End Mismatch: element 'br' not closed", 1, 23],
        [:end_element, :br],
        [:text, " text"],
        [:end_element, :html]
      ])
  end

  # NOTE: Not closed hr tag will have to be closed immediately
  def test_nested_not_closed_hr_tag
    html = %{<html>A terminated <hr> text</html>}
    parse_compare(html,
      [
        [:start_element, :html],
        [:text, "A terminated "],
        [:start_element, :hr],
        [:error, "Start End Mismatch: element 'hr' not closed", 1, 23],
        [:end_element, :hr],
        [:text, " text"],
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

end

# this can be renamed or moved around
class SaxSmartHtmlTest < SaxSmartTest

  def test_html_no_term
    html = %{
<!DOCTYPE HTML PUBLIC "-//IETF//DTD HTML//EN">
<html>
  <head>
    <title>Sax</title>
  </head>
  <body>
    <p>great</p>
    <img src="x.jpg">
  </body>
</html>
}
    parse_compare(html, [
                         [:doctype, " HTML PUBLIC \"-//IETF//DTD HTML//EN\""],
                         [:start_element, :html],
                         [:start_element, :head],
                         [:start_element, :title],
                         [:text, "Sax"],
                         [:end_element, :title],
                         [:end_element, :head],
                         [:start_element, :body],
                         [:start_element, :p],
                         [:text, "great"],
                         [:end_element, :p],
                         [:start_element, :img],
                         [:attr, :src, "x.jpg"],
                         [:end_element, :body],
                         [:end_element, :html],
                        ])
  end

  def test_html_invalid_element
    html = %{
<html>
  <budy>Hello</budy>
</html>
}
    parse_compare(html, [
                         [:start_element, :html],
                         [:error, "Invalid Element: budy is not a valid element type for a HTML document type.", 3, 9],
                         [:start_element, :budy],
                         [:text, "Hello"],
                         [:end_element, :budy],
                         [:end_element, :html]
                        ])
  end

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
    parse_compare(html, [
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
    parse_compare(html, [
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

  def test_html_nested
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
                  [[:start_element, :html], [:start_element, :body]] +
                  [[:start_element, :div]] * 20 +
                  [[:text, "\nWord\n    "]] +
                  [[:end_element, :div]] * 20 +
                  [[:end_element, :body],[:end_element, :html]])
  end
end # SaxSmartHtmlTest

