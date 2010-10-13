<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
version="1.0">

  <!--
      Driver file for the transformation of Kerrighed doc to xhtml.
    
      Mostly inspired by RefDB driver
       -->
  <xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/docbook.xsl"/>
  <!-- xsl:import href="http://docbook.sourceforge.net/release/xsl/current/xhtml/chunk.xsl"/ -->

  <!--
       the following templates add refentries to the chapter TOCs
       -->

  <xsl:template match="sect1" mode="toc">
    <xsl:param name="toc-context" select="."/>
    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="sect2|refentry                                          |bridgehead[$bridgehead.in.toc != 0]"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="sect2" mode="toc">
    <xsl:param name="toc-context" select="."/>

    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="sect3|refentry                                          |bridgehead[$bridgehead.in.toc != 0]"/>
    </xsl:call-template>
  </xsl:template>

  <!--
       add refsects and synopsis to the TOCs
       avoid duplicate refentrytitles
       -->

  <xsl:template match="refentry" mode="toc">
    <xsl:param name="toc-context" select="."/>

    <xsl:variable name="refmeta" select=".//refmeta"/>
    <xsl:variable name="refentrytitle" select="$refmeta//refentrytitle"/>
    <xsl:variable name="refnamediv" select=".//refnamediv"/>
    <xsl:variable name="refname" select="$refnamediv//refname"/>
    <xsl:variable name="refdesc" select="$refnamediv//refdescriptor"/>
    <xsl:variable name="title">
      <xsl:choose>
        <xsl:when test="$refentrytitle">
          <xsl:apply-templates select="$refentrytitle[1]" mode="titleabbrev.markup"/>
        </xsl:when>
        <xsl:when test="$refdesc">
          <xsl:apply-templates select="$refdesc" mode="titleabbrev.markup"/>
        </xsl:when>
        <xsl:when test="$refname">
          <xsl:apply-templates select="$refname[1]" mode="titleabbrev.markup"/>
        </xsl:when>
      </xsl:choose>
    </xsl:variable>

    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="refsect1 | refsect2 | refsect3"/>
    </xsl:call-template>
    
  </xsl:template>

  <xsl:template match="refsect1" mode="toc">
    <xsl:param name="toc-context" select="."/>
    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="refsect2"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="refsect2" mode="toc">
    <xsl:param name="toc-context" select="."/>

    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="refsect3"/>
    </xsl:call-template>
  </xsl:template>

  <xsl:template match="refsect3" mode="toc">
    <xsl:param name="toc-context" select="."/>

    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="refsect4"/>
    </xsl:call-template>
  </xsl:template>

  <!--
       The title must be generated before passing it to the toc-generating
       code. This one causes a warning during the transformation which
       you can safely ignore as long as the result is correct
       -->

  <xsl:template match="refsynopsisdiv" mode="toc">
    <xsl:param name="toc-context" select="."/>

    <xsl:variable name="title">
      <xsl:call-template name="gentext">
        <xsl:with-param name="key" select="'RefSynopsisDiv'"/>
      </xsl:call-template>
    </xsl:variable>

    <xsl:call-template name="subtoc">
      <xsl:with-param name="toc-context" select="$toc-context"/>
      <xsl:with-param name="nodes" select="$title"/>
    </xsl:call-template>
  </xsl:template>
  

  <!-- include kerrighed.org headers fields -->
  <xsl:template name="user.head.content">
     <meta name="keywords" content="V3.0.0 User Manual" />
     <link rel="shortcut icon" href="/favicon.ico" />
     <link rel="search" type="application/opensearchdescription+xml" href="/wiki/opensearch_desc.php" title="Kerrighed (English)" />
     <link rel="copyright" href="http://www.gnu.org/copyleft/fdl.html" />
  </xsl:template>

  <!-- include kerrighed.org header -->
  <xsl:template name="user.header.content">
     <xsl:text disable-output-escaping="yes">&#60;div id="global"&#62;</xsl:text>

        <div id="head" class="box">
           <div id="headBody" class="boxBody">
              <div id="logo">
                 <a href="/wiki/index.php"><img src="../images/kerrighed.png" alt="Kerrighed logo"></img></a>
              </div>
              <div id="title"><h1><xsl:apply-templates select="/" mode="title.markup" /></h1></div>
              <div class="portlet" id="p-tb">
                 <h5>Toolbox</h5>
                 <ul>
                    <li id="t-print">
                       <a title="Printable version" href="javascript:print()"><img alt="Printable version icon" src="/images/gartoon_print.png" />
                       </a>
                    </li>
                 </ul>
                 <form name="searchform" action="/wiki/index.php/Special:Search" id="searchform">
                    <input type="submit" name="go" class="searchButton" id="searchGoButton" value="Go" />
                    <input type="image" name="fulltext" class="searchButton" id="searchButton" value="Search" src="/images/gartoon_search.png" />
                    <input id="searchInput" name="search" type="text" accesskey="f" value="" />
                 </form>
              </div>
           </div>
        </div>

     <xsl:text disable-output-escaping="yes">&#60;div id="globalBody"&#62;</xsl:text>
     <xsl:text disable-output-escaping="yes">&#60;div id="bodyContent" class="box"&#62;</xsl:text>
     <xsl:text disable-output-escaping="yes">&#60;div id="boxBody"&#62;</xsl:text>
        <a name="top" id="top"></a>
  </xsl:template>

  <!-- include kerrighed.org footer -->
  <xsl:template name="user.footer.content">
     <xsl:text disable-output-escaping="yes">&#60;/div&#62;</xsl:text>
     <xsl:text disable-output-escaping="yes">&#60;/div&#62;</xsl:text>
     <xsl:text disable-output-escaping="yes">&#60;/div&#62;</xsl:text>

        <div class="visualClear"></div>
        <div id="footer" class="box">
           <div class="boxBody">
              <ul class="flat">
                 <li id="f-about"><a href="/wiki/index.php/Kerrighed:About" title="Kerrighed:About">About Kerrighed</a></li>
                 <li id="f-disclaimer"><a href="/wiki/index.php/Kerrighed:General_disclaimer" title="Kerrighed:General disclaimer">Disclaimers</a></li>
              </ul>
           </div>
        </div>

     <xsl:text disable-output-escaping="yes">&#60;/div&#62;</xsl:text>
  </xsl:template>



  <!-- some overrides of parameters defined in param.xsl -->
  <xsl:param name="html.stylesheet" select="'/manual.css'"/>
  <xsl:param name="funcsynopsis.style" select="'ansi'"/>
  <xsl:param name="graphic.default.extension" select="'png'"/>
  <xsl:param name="chunker.output.encoding" select="'US-ASCII'"/>

  <!-- xsl:param name="man.endnotes.list.enabled">1</xsl:param>
  <xsl:param name="man.base.url.for.relative.links">[set $man.base.url.for.relative.links]/</xsl:param -->

</xsl:stylesheet>
