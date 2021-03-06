/***************************************************************
 *
 * Copyright (C) 2014, Condor Team, Computer Sciences Department,
 * University of Wisconsin-Madison, WI.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License.  You may
 * obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ***************************************************************/

#include "condor_common.h"
#include "ad_printmask.h"
#include "condor_string.h"	// for getline
#include "tokener.h"
#include <string>

const char * SimpleFileInputStream::nextline()
{
	// getline from condor_string.h automatically collapses line continuations.
	// and uses a static internal buffer to hold the latest line. 
	const char * line = getline_trim(file, lines_read);
	return line;
}


typedef struct {
	const char * key;
	int          value;
	int          options;
} Keyword;
typedef case_sensitive_sorted_tokener_lookup_table<Keyword> KeywordTable;

enum {
	kw_AS=1,
	kw_AUTO,
	kw_PRINTF,
	kw_PRINTAS,
	kw_NOPREFIX,
	kw_NOSUFFIX,
	kw_TRUNCATE,
	kw_WIDTH,
	kw_LEFT,
	kw_RIGHT,
	kw_OR,
	kw_ALWAYS,
};

#define KW(a) { #a, kw_##a, 0 }
static const Keyword SelectKeywordItems[] = {
	KW(ALWAYS),
	KW(AS),
	KW(AUTO),
	KW(LEFT),
	KW(NOPREFIX),
	KW(NOSUFFIX),
	KW(OR),
	KW(PRINTAS),
	KW(PRINTF),
	KW(RIGHT),
	KW(TRUNCATE),
	KW(WIDTH),
};
#undef KW
static const KeywordTable SelectKeywords = SORTED_TOKENER_TABLE(SelectKeywordItems);

enum {
	gw_AS=1,
	gw_ASCENDING,
	gw_DECENDING,
};
#define GW(a) { #a, gw_##a, 0 }
static const Keyword GroupKeywordItems[] = {
	GW(AS),
	GW(ASCENDING),
	GW(DECENDING),
};
#undef GW
static const KeywordTable GroupKeywords = SORTED_TOKENER_TABLE(GroupKeywordItems);

static void unexpected_token(std::string & message, const char * tag, SimpleInputStream & stream, tokener & toke)
{
	std::string tok; toke.copy_token(tok);
	formatstr_cat(message, "%s was unexpected at line %d offset %d in %s\n",
		tok.c_str(), stream.count_of_lines_read(), (int)toke.offset(), tag);
}

static void expected_token(std::string & message, const char * reason, const char * tag, SimpleInputStream & stream, tokener & toke)
{
	std::string tok; toke.copy_token(tok);
	formatstr_cat(message, "expected %s at line %d offset %d in %s\n",
		reason, stream.count_of_lines_read(), (int)toke.offset(), tag);
}

// Read a stream a line at a time, and parse it to fill out the print mask,
// header, group_by, where expression, and projection attributes.
//
int SetAttrListPrintMaskFromStream (
	SimpleInputStream & stream, // in: fetch lines from this stream until nextline() returns NULL
	const CustomFormatFnTable & FnTable, // in: table of custom output functions for SELECT
	AttrListPrintMask & mask, // out: columns and headers set in SELECT
	printmask_headerfooter_t & headfoot, // out: header and footer flags set in SELECT or SUMMARY
	printmask_aggregation_t & aggregate, // out: aggregation mode in SELECT
	std::vector<GroupByKeyInfo> & group_by, // out: ordered set of attributes/expressions in GROUP BY
	std::string & where_expression, // out: classad expression from WHERE
	StringList & attrs, // out ClassAd attributes referenced in mask or group_by outputs
	std::string & error_message) // out, if return is non-zero, this will be an error message
{
	ClassAd ad; // so we can GetExprReferences
	enum section_t { NOWHERE=0, SELECT, SUMMARY, WHERE, GROUP};
	enum cust_t { PRINTAS_STRING, PRINTAS_INT, PRINTAS_FLOAT };

	bool label_fields = false;
	const char * labelsep = " = ";
	const char * prowpre = NULL;
	const char * pcolpre = " ";
	const char * pcolsux = NULL;
	const char * prowsux = "\n";
	mask.SetAutoSep(prowpre, pcolpre, pcolsux, prowsux);

	error_message.clear();
	aggregate = PR_NO_AGGREGATION;

	printmask_headerfooter_t usingHeadFoot = (printmask_headerfooter_t)(HF_CUSTOM | HF_NOSUMMARY);
	section_t sect = SELECT;
	tokener toke("");
	while (toke.set(stream.nextline())) {
		if ( ! toke.next())
			continue;

		if (toke.matches("#")) continue;

		if (toke.matches("SELECT"))	{
			while (toke.next()) {
				if (toke.matches("FROM")) {
					if (toke.next()) {
						if (toke.matches("AUTOCLUSTER")) {
							aggregate = PR_FROM_AUTOCLUSTER;
						} else {
							std::string aa; toke.copy_token(aa);
							formatstr_cat(error_message, "Warning: Unknown header argument %s for SELECT FROM\n", aa.c_str());
						}
					} else {
						expected_token(error_message, "data set name after FROM", "SELECT", stream, toke);
					}
				} else if (toke.matches("UNIQUE")) {
					aggregate = PR_COUNT_UNIQUE;
				} else if (toke.matches("BARE")) {
					usingHeadFoot = HF_BARE;
				} else if (toke.matches("NOTITLE")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOTITLE);
				} else if (toke.matches("NOHEADER")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOHEADER);
				} else if (toke.matches("NOSUMMARY")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOSUMMARY);
				} else if (toke.matches("LABEL")) {
					label_fields = true;
				} else if (label_fields && toke.matches("SEPARATOR")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); labelsep = mask.store(tmp.c_str()); }
				} else if (toke.matches("RECORDPREFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); prowpre = mask.store(tmp.c_str()); }
				} else if (toke.matches("RECORDSUFFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); prowsux = mask.store(tmp.c_str()); }
				} else if (toke.matches("FIELDPREFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); pcolpre = mask.store(tmp.c_str()); }
				} else if (toke.matches("FIELDSUFFIX")) {
					if (toke.next()) { std::string tmp; toke.copy_token(tmp); collapse_escapes(tmp); pcolsux = mask.store(tmp.c_str()); }
				} else {
					std::string aa; toke.copy_token(aa);
					formatstr_cat(error_message, "Warning: Unknown header argument %s for SELECT\n", aa.c_str());
				}
			}
			mask.SetAutoSep(prowpre, pcolpre, pcolsux, prowsux);
			sect = SELECT;
			continue;
		} else if (toke.matches("WHERE")) {
			sect = WHERE;
			if ( ! toke.next()) continue;
		} else if (toke.matches("GROUP")) {
			sect = GROUP;
			if ( ! toke.next() || (toke.matches("BY") && ! toke.next())) continue;
		} else if (toke.matches("SUMMARY")) {
			usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot & ~HF_NOSUMMARY);
			while (toke.next()) {
				if (toke.matches("STANDARD")) {
					// attrs.insert(ATTR_JOB_STATUS);
				} else if (toke.matches("NONE")) {
					usingHeadFoot = (printmask_headerfooter_t)(usingHeadFoot | HF_NOSUMMARY);
				} else {
					std::string aa; toke.copy_token(aa);
					formatstr_cat(error_message, "Unknown argument %s for SELECT\n", aa.c_str());
				}
			}
			sect = SUMMARY;
			continue;
		}

		switch (sect) {
		case SELECT: {
			toke.mark();
			std::string attr;
			std::string name;
			int opts = 0;
			int def_opts = FormatOptionAutoWidth | FormatOptionNoTruncate;
			const char * def_fmt = "%v";
			const char * fmt = def_fmt;
			int wid = 0;
			bool width_from_label = true;
			CustomFormatFn cust;

			bool got_attr = false;
			while (toke.next()) {
				const Keyword * pkw = SelectKeywords.lookup_token(toke);
				if ( ! pkw)
					continue;

				// next token is a keyword, if we havent set the attribute yet
				// it's everything up to the current token.
				int kw = pkw->value;
				if ( ! got_attr) {
					toke.copy_marked(attr);
					got_attr = true;
				}

				switch (kw) {
				case kw_AS: {
					if (toke.next()) {
						toke.copy_token(name);
						if (toke.is_quoted_string()) { collapse_escapes(name); }
					} else {
						expected_token(error_message, "column name after AS", "SELECT", stream, toke);
					}
					toke.mark_after();
				} break;
				case kw_PRINTF: {
					if (toke.next()) {
						std::string val; toke.copy_token(val);
						fmt = mask.store(val.c_str());
					} else {
						expected_token(error_message, "format after PRINTF", "SELECT", stream, toke);
					}
				} break;
				case kw_PRINTAS: {
					if (toke.next()) {
						const CustomFormatFnTableItem * pcffi = FnTable.lookup_token(toke);
						if (pcffi) {
							cust = pcffi->cust;
							fmt = pcffi->printfFmt;
							if (fmt) fmt = mask.store(fmt);
							//cust_type = pcffi->cust;
							const char * pszz = pcffi->extra_attribs;
							if (pszz) {
								size_t cch = strlen(pszz);
								while (cch > 0) {
									if ( ! attrs.contains_anycase(pszz)) attrs.insert(pszz);
									pszz += cch+1; cch = strlen(pszz);
								}
							}
						} else {
							std::string aa; toke.copy_token(aa);
							formatstr_cat(error_message, "Unknown argument %s for PRINTAS\n", aa.c_str());
						}
					} else {
						expected_token(error_message, "function name after PRINTAS", "SELECT", stream, toke);
					}
				} break;
				case kw_OR: {
					if (toke.next()) {
						std::string val; toke.copy_token(val);
						const char alt_chars[] = " ?*.-_#0";
						if (val.length() > 0 && strchr(alt_chars, val[0])) {
							int ix = (int)(strchr(alt_chars, val[0]) - &alt_chars[0]);
							opts |= (ix*AltQuestion);
							if (val.length() > 1 && val[0] == val[1]) { opts |= AltWide; }
						} else {
							formatstr_cat(error_message, "Unknown argument %s for OR\n", val.c_str());
						}
					} else {
						expected_token(error_message, "? or ?? after OR", "SELECT", stream, toke);
					}
				} break;
				case kw_NOSUFFIX: {
					opts |= FormatOptionNoSuffix;
				} break;
				case kw_NOPREFIX: {
					opts |= FormatOptionNoPrefix;
				} break;
				case kw_LEFT: {
					opts |= FormatOptionLeftAlign;
				} break;
				case kw_RIGHT: {
					opts &= ~FormatOptionLeftAlign;
				} break;
				case kw_TRUNCATE: {
					opts &= ~FormatOptionNoTruncate;
					def_opts &= ~FormatOptionNoTruncate;
				} break;
				case kw_ALWAYS: {
					opts |= FormatOptionAlwaysCall;
				} break;
				case kw_WIDTH: {
					if (toke.next()) {
						std::string val; toke.copy_token(val);
						if (toke.matches("AUTO")) {
							opts |= FormatOptionAutoWidth;
							def_opts |= (FormatOptionAutoWidth | FormatOptionNoTruncate);
						} else {
							wid = atoi(val.c_str());
							def_opts &= ~(FormatOptionAutoWidth | FormatOptionNoTruncate);
							width_from_label = false;
							//PRAGMA_REMIND("TJ: decide how LEFT & RIGHT interact with pos and neg widths."
							if (fmt == def_fmt) {
								fmt = NULL;
							}
						}
					} else {
						expected_token(error_message, "number or AUTO after WIDTH", "SELECT", stream, toke);
					}
				} break;
				default:
					unexpected_token(error_message, "SELECT", stream, toke);
				break;
				} // switch
			} // while

			if ( ! got_attr) { attr = toke.content(); }
			trim(attr);
			if (attr.empty() || attr[0] == '#') continue;

			opts |= def_opts;

			const char * lbl = name.empty() ? attr.c_str() : name.c_str();
			if (label_fields) {
				// build a format string that contains the label
				std::string label(lbl);
				if (labelsep) { label += labelsep; }
				if (fmt) { label += fmt; } 
				else {
					label += "%";
					if (wid) {
						if (opts & FormatOptionNoTruncate)
							formatstr_cat(label, "%d", wid);
						else
							formatstr_cat(label, "%d.%d", wid, wid < 0 ? -wid : wid);
					}
					label += cust ? "s" : "v";
				}
				lbl = mask.store(label.c_str());
				fmt = lbl;
				wid = 0;
			} else {
				if (width_from_label) { wid = 0 - (int)strlen(lbl); }
				mask.set_heading(lbl);
				lbl = NULL;
				if (cust) {
					lbl = fmt;
				} else if ( ! fmt) {
					// if we get to here and there is no format, that means that
					// a width was specified, but no custom or printf format. so we
					// need to manufacture a printf format based on the given width.
					if ( ! wid) { fmt = def_fmt; }
					else {
						char tmp[40] = "%"; char *p = tmp+1;
						if (wid < 0) { opts |= FormatOptionLeftAlign; wid = -wid; *p++ = '-'; }
						p += sprintf(p, "%d", wid);
						if ( ! (opts & FormatOptionNoTruncate)) p += sprintf(p, ".%d", wid);
						*p++ = 'v'; *p = 0;
						fmt = mask.store(tmp);
					}
				}
			}
			if (cust) {
				mask.registerFormat (lbl, wid, opts, cust, attr.c_str());
			} else {
				mask.registerFormat(fmt, wid, opts, attr.c_str());
			}
			ad.GetExprReferences(attr.c_str(), NULL, &attrs);
		}
		break;

		case WHERE: {
			toke.copy_to_end(where_expression);
			trim(where_expression);
		}
		break;

		case SUMMARY: {
		}
		break;

		case GROUP: {
			toke.mark();
			GroupByKeyInfo key;

			// in case we end up finding no keywords, copy the remainder of the line now as the expression
			toke.copy_to_end(key.expr);
			bool got_expr = false;

			while (toke.next()) {
				const Keyword * pgw = GroupKeywords.lookup_token(toke);
				if ( ! pgw)
					continue;

				// got a keyword
				int gw = pgw->value;
				if ( ! got_expr) {
					toke.copy_marked(key.expr);
					got_expr = true;
				}

				switch (gw) {
				case gw_AS: {
					if (toke.next()) { toke.copy_token(key.name); }
					toke.mark_after();
				} break;
				case gw_DECENDING: {
					key.decending = true;
					toke.mark_after();
				} break;
				case gw_ASCENDING: {
					key.decending = false;
					toke.mark_after();
				} break;
				default:
					unexpected_token(error_message, "GROUP BY", stream, toke);
				break;
				} // switch
			} // while toke.next

			trim(key.expr);
			if (key.expr.empty() || key.expr[0] == '#')
				continue;

			if ( ! ad.GetExprReferences(key.expr.c_str(), NULL, &attrs)) {
				formatstr_cat(error_message, "GROUP BY expression is not valid: %s\n", key.expr.c_str());
			} else {
				group_by.push_back(key);
			}
		}
		break;

		default:
		break;
		}
	}

	headfoot = usingHeadFoot;

	return 0;
}
