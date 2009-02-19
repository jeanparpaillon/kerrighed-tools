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
  
  <!-- some overrides of parameters defined in param.xsl -->
  <xsl:param name="html.stylesheet" select="'manual.css'"/>
  <xsl:param name="funcsynopsis.style" select="'ansi'"/>
  <xsl:param name="graphic.default.extension" select="'png'"/>

  <!-- xsl:param name="man.endnotes.list.enabled">1</xsl:param>
  <xsl:param name="man.base.url.for.relative.links">[set $man.base.url.for.relative.links]/</xsl:param -->

</xsl:stylesheet>
