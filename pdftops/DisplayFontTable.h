//========================================================================
//
// DisplayFontTable.h
//
// Copyright 2001-2003 Glyph & Cog, LLC
//
//========================================================================

static struct {
  const char *name;
  const char *xlfd;
  const char *encoding;
} displayFontTab[] = {
  {"Courier",               "-*-courier-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",         "Latin1"},
  {"Courier-Bold",          "-*-courier-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",           "Latin1"},
  {"Courier-BoldOblique",   "-*-courier-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",           "Latin1"},
  {"Courier-Oblique",       "-*-courier-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",         "Latin1"},
  {"Helvetica",             "-*-helvetica-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",       "Latin1"},
  {"Helvetica-Bold",        "-*-helvetica-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",         "Latin1"},
  {"Helvetica-BoldOblique", "-*-helvetica-bold-o-normal-*-%s-*-*-*-*-*-iso8859-1",         "Latin1"},
  {"Helvetica-Oblique",     "-*-helvetica-medium-o-normal-*-%s-*-*-*-*-*-iso8859-1",       "Latin1"},
  {"Symbol",                "-*-symbol-medium-r-normal-*-%s-*-*-*-*-*-adobe-fontspecific", "Symbol"},
  {"Times-Bold",            "-*-times-bold-r-normal-*-%s-*-*-*-*-*-iso8859-1",             "Latin1"},
  {"Times-BoldItalic",      "-*-times-bold-i-normal-*-%s-*-*-*-*-*-iso8859-1",             "Latin1"},
  {"Times-Italic",          "-*-times-medium-i-normal-*-%s-*-*-*-*-*-iso8859-1",           "Latin1"},
  {"Times-Roman",           "-*-times-medium-r-normal-*-%s-*-*-*-*-*-iso8859-1",           "Latin1"},
  {"ZapfDingbats",          "-*-zapfdingbats-medium-r-normal-*-%s-*-*-*-*-*-*-*",          "ZapfDingbats"},
  {NULL}
};
