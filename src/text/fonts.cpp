#include <cmath>
#include <bit>

#include "hb.h"
#include "hb-ft.h"

#include "fonts.hpp"
#include "parsers.hpp"
#include "simple_fs.hpp"
#include "system_state.hpp"
#include "bmfont.hpp"

namespace text {

uint16_t b_to_u16(uint8_t const* bytes) {
	return (uint16_t(bytes[0]) << 8) | (uint16_t(bytes[1]));
}

uint32_t b_to_u32(uint8_t const* bytes) {
	return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) | (uint32_t(bytes[2]) << 8) | (uint32_t(bytes[3]));
}

int16_t b_to_i16(uint8_t const* bytes) {
	return static_cast<int16_t>((uint16_t(bytes[0]) << 8) | (uint16_t(bytes[1])));
}

int32_t feature_offset(uint8_t const* table_base) {
	return b_to_u16(table_base + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t));
}
int32_t script_offset(uint8_t const* table_base) {
	return b_to_u16(table_base + sizeof(uint16_t) + sizeof(uint16_t));
}
int32_t lookup_offset(uint8_t const* table_base) {
	return b_to_u16(table_base + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t));
}

struct opentype_tag {
	char tag_data[4];
};

static_assert(sizeof(opentype_tag) == 4);

namespace script_list_table {
template<typename F>
void for_each_script_table(uint8_t const* script_list_table_base, F&& func) {
	auto count = b_to_u16(script_list_table_base + 0);

	auto script_record_ptr = script_list_table_base + sizeof(uint16_t);

	for(uint16_t i = 0; i < count; ++i) {
		auto tag_ptr = reinterpret_cast<opentype_tag const*>(script_record_ptr);
		auto offset = b_to_u16(script_record_ptr + sizeof(opentype_tag));
		func(*tag_ptr, script_list_table_base + offset);
		script_record_ptr += sizeof(uint16_t) + sizeof(opentype_tag);
	}
}
}

namespace script_table {
template<typename F>
void for_each_lang_sys_table(uint8_t const* script_table_base, F&& func) {
	auto langSysCount = b_to_u16(script_table_base + sizeof(uint16_t));
	auto LangSysRecord_ptr = script_table_base + sizeof(uint16_t) + sizeof(uint16_t);

	for(uint16_t i = 0; i < langSysCount; ++i) {
		auto tag_ptr = reinterpret_cast<opentype_tag const*>(LangSysRecord_ptr);
		auto offset = b_to_u16(LangSysRecord_ptr + sizeof(opentype_tag));
		func(*tag_ptr, script_table_base + offset);
		LangSysRecord_ptr += sizeof(uint16_t) + sizeof(opentype_tag);
	}
}
uint8_t const* default_lang_sys_table(uint8_t const* script_table_base) {
	auto defaultLangSysOffset = b_to_i16(script_table_base + 0);
	if(defaultLangSysOffset != 0)
		return script_table_base + defaultLangSysOffset;
	else
		return nullptr;
}
}

namespace lang_sys_table {
uint16_t required_feature(uint8_t const* lang_sys_table) {
	return b_to_u16(lang_sys_table + sizeof(uint16_t));
}
template<typename F>
void for_each_feature_index(uint8_t const* lang_sys_table, F&& func) {
	auto featureIndexCount = b_to_u16(lang_sys_table + sizeof(uint16_t) + sizeof(uint16_t));
	auto featureIndices_ptr = lang_sys_table + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t);

	for(uint16_t i = 0; i < featureIndexCount; ++i) {
		func(b_to_u16(featureIndices_ptr));
		featureIndices_ptr += sizeof(uint16_t);
	}
}
}

namespace feature_list_table{
template<typename F>
void for_each_feature_table(uint8_t const* feature_list_table_base, F&& func) {
	auto featureCount = b_to_u16(feature_list_table_base + 0);
	
	auto FeatureRecord_ptr = feature_list_table_base + sizeof(uint16_t);

	for(uint16_t i = 0; i < featureCount; ++i) {
		auto tag_ptr = reinterpret_cast<opentype_tag const*>(FeatureRecord_ptr);
		auto offset = b_to_u16(FeatureRecord_ptr + sizeof(opentype_tag));
		func(*tag_ptr, feature_list_table_base + offset, i);
		FeatureRecord_ptr += sizeof(uint16_t) + sizeof(opentype_tag);
	}
}
struct tagged_feature {
	opentype_tag tag;
	uint8_t const* feature_table;
};
tagged_feature get_feature_table(uint8_t const* feature_list_table_base, uint16_t index) {
	auto featureCount = b_to_u16(feature_list_table_base + 0);
	auto FeatureRecord_ptr = feature_list_table_base + sizeof(uint16_t) + index * (sizeof(uint16_t) + sizeof(opentype_tag));
	if(index < featureCount) {
		return tagged_feature{
			*reinterpret_cast<opentype_tag const*>(FeatureRecord_ptr),
			feature_list_table_base + b_to_u16(FeatureRecord_ptr + sizeof(opentype_tag)) };
	} else {
		return tagged_feature{ opentype_tag{ "NON" }, nullptr };
	}
}
}

namespace feature_table {
template<typename F>
void for_each_lookup_list_index(uint8_t const* feature_table, F&& func) {
	auto lookupIndexCount = b_to_u16(feature_table + sizeof(uint16_t));
	auto lookupListIndices_ptr = feature_table + sizeof(uint16_t) + sizeof(uint16_t);

	for(uint16_t i = 0; i < lookupIndexCount; ++i) {
		func(b_to_u16(lookupListIndices_ptr));
		lookupListIndices_ptr += sizeof(uint16_t);
	}
}
}

namespace lookup_list_table {
uint16_t count(uint8_t const* lookup_list_table) {
	return b_to_u16(lookup_list_table + 0);
}
uint8_t const* get_lookup_table(uint8_t const* lookup_list_table, uint16_t index) {
	auto lookupCount = b_to_u16(lookup_list_table + 0);
	auto lookupOffsets_ptr = lookup_list_table + sizeof(uint16_t);
	if(index < lookupCount) {
		return lookup_list_table + b_to_u16(lookupOffsets_ptr + index * sizeof(uint16_t));
	} else {
		return nullptr;
	}
}
}

namespace lookup_table {
uint16_t get_lookup_type(uint8_t const* lookup_table) {
	return b_to_u16(lookup_table + 0);
}
uint16_t get_lookup_flag(uint8_t const* lookup_table) {
	return b_to_u16(lookup_table + sizeof(uint16_t));
}
uint16_t get_sub_table_count(uint8_t const* lookup_table) {
	return b_to_u16(lookup_table + sizeof(uint16_t) + sizeof(uint16_t));
}
uint8_t const* get_sub_table(uint8_t const* lookup_table, uint16_t index) {
	auto sub_table_count = b_to_u16(lookup_table + sizeof(uint16_t) + sizeof(uint16_t));
	auto subtableOffsets_ptr = lookup_table + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t);
	if(index < sub_table_count) {
		return lookup_table + b_to_u16(subtableOffsets_ptr + index * sizeof(uint16_t));
	} else {
		return nullptr;
	}
}
}

namespace coverage_table {
uint16_t get_format(uint8_t const* coverage_table) {
	return b_to_u16(coverage_table + 0);
}
uint16_t format1_glyph_count(uint8_t const* coverage_table) {
	return b_to_u16(coverage_table + sizeof(uint16_t));
}
int32_t format1_glyph_index(uint8_t const* coverage_table, uint16_t glyph_in) {
	auto count = format1_glyph_count(coverage_table);
	auto data_ptr = coverage_table + sizeof(uint16_t) + sizeof(uint16_t);
	for(int32_t i = 0; i < int32_t(count); ++i) {
		auto g = b_to_u16(data_ptr);
		if(g == glyph_in)
			return i;
		data_ptr += sizeof(uint16_t);
	}
	return -1;
}
uint16_t format2_range_count(uint8_t const* coverage_table) {
	return b_to_u16(coverage_table + sizeof(uint16_t));
}
int32_t format2_glyph_index(uint8_t const* coverage_table, uint16_t glyph_in) {
	auto count = format2_range_count(coverage_table);
	auto data_ptr = coverage_table + sizeof(uint16_t) + sizeof(uint16_t);
	for(int32_t i = 0; i < int32_t(count); ++i) {
		auto start_glyph = b_to_u16(data_ptr + 0);
		auto end_glyph = b_to_u16(data_ptr + sizeof(uint16_t));
		auto startCoverageIndex = b_to_u16(data_ptr + sizeof(uint16_t) + sizeof(uint16_t));

		if(start_glyph <= glyph_in && glyph_in <= end_glyph)
			return startCoverageIndex + glyph_in - start_glyph;

		data_ptr += sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t);
	}
	return -1;
}
int32_t glyph_index(uint8_t const* coverage_table, uint16_t glyph_in) {
	auto fmt = coverage_table::get_format(coverage_table);
	if(fmt == 1) {
		return coverage_table::format1_glyph_index(coverage_table, glyph_in);
	} else if(fmt == 2) {
		return coverage_table::format2_glyph_index(coverage_table, glyph_in);
	} else {
		return -1;
	}
}
}

namespace gsub {
uint32_t perform_single_substitution(uint8_t const* sub_table, uint32_t glyph_index_in) {
	auto format = b_to_u16(sub_table + 0);
	if(format == 1) {
		auto coverageOffset = b_to_u16(sub_table + sizeof(uint16_t));
		auto deltaGlyphID = b_to_i16(sub_table + sizeof(uint16_t) + sizeof(uint16_t));
		auto ct = sub_table + coverageOffset;
		
		auto indx = coverage_table::glyph_index(ct, uint16_t(glyph_index_in));
		if(indx == -1)
			return glyph_index_in;
		else
			return uint32_t(int32_t(glyph_index_in) + deltaGlyphID);
		
	} else if(format == 2) {
		auto coverageOffset = b_to_u16(sub_table + sizeof(uint16_t));
		auto glyphCount = b_to_u16(sub_table + sizeof(uint16_t) + sizeof(uint16_t));
		auto substituteGlyphIDs = sub_table + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t);
		auto ct = sub_table + coverageOffset;

		auto indx = coverage_table::glyph_index(ct, uint16_t(glyph_index_in));
		if(indx == -1)
			return glyph_index_in;

		assert(indx < int32_t(glyphCount));
		return uint32_t(b_to_u16(substituteGlyphIDs + sizeof(uint16_t) * indx));
	} else {
		return glyph_index_in;
	}
}

uint8_t const* find(uint8_t const* file_data) {
	auto num_tables = b_to_u16(file_data + sizeof(uint32_t));
	auto table_records = file_data + sizeof(uint32_t) + sizeof(uint16_t) * 4;
	for(uint16_t i = 0; i < num_tables; ++i) {
		if(char(table_records[0]) == 'G' && char(table_records[1]) == 'S' && char(table_records[2]) == 'U' && char(table_records[3]) == 'B') {
			auto offset = b_to_u32(table_records + sizeof(uint32_t) * 2);
			return file_data + offset;
		}
		table_records += sizeof(uint32_t) * 4;
	}
	return nullptr;
}

uint32_t perform_glyph_subs(uint8_t const* gsub_table, std::vector<uint16_t> const& lookups, uint32_t glyph_in) {
	auto lo = gsub_table + lookup_offset(gsub_table);

	for(auto i : lookups) {
		auto lt = lookup_list_table::get_lookup_table(lo, i);
		if(lt && lookup_table::get_lookup_type(lt) == 1) {
			auto num_st = lookup_table::get_sub_table_count(lt);
			for(uint16_t j = 0; j < num_st; ++j) {
				if(auto st = lookup_table::get_sub_table(lt, j); st) {
					auto new_glyph = perform_single_substitution(st, glyph_in);
					if(glyph_in != new_glyph)
						return new_glyph;
				}
			}
		}
	}

	return glyph_in;
}

/*
void dump_gsub(uint8_t const* data) {
	OutputDebugStringA("Script List Table\n");
	auto so = data + script_offset(data);
	script_list_table::for_each_script_table(so, [](opentype_tag const& t, uint8_t const* d) {
		std::string tag = "\t[";
		tag += t.tag_data[0];
		tag += t.tag_data[1];
		tag += t.tag_data[2];
		tag += t.tag_data[3];
		tag += "] script table\n";
		OutputDebugStringA(tag.c_str());

		auto def_table = script_table::default_lang_sys_table(d);
		if(def_table) {
			OutputDebugStringA("\t Default language system table:\n");
			auto feat = lang_sys_table::required_feature(def_table);
			if(feat != uint16_t(0xFFFF)) {
				std::string fr = std::string("\t\t required feature: ") + std::to_string(feat) + "\n";
				OutputDebugStringA(fr.c_str());
			} else {
				OutputDebugStringA("\t\t no required feature\n");
			}
			lang_sys_table::for_each_feature_index(def_table, [](uint16_t id) {
				std::string f2 = "\t\t feature: " + std::to_string(id) + "\n";
				OutputDebugStringA(f2.c_str());
			});
		} else {
			OutputDebugStringA("\t No default language system table\n");
		}

		script_table::for_each_lang_sys_table(d, [](opentype_tag const& t2, uint8_t const* d2) {
			std::string tag2 = "\t Language system table [";
			tag2 += t2.tag_data[0];
			tag2 += t2.tag_data[1];
			tag2 += t2.tag_data[2];
			tag2 += t2.tag_data[3];
			tag2 += "]\n";
			OutputDebugStringA(tag2.c_str());

			auto feat = lang_sys_table::required_feature(d2);
			if(feat != uint16_t(0xFFFF)) {
				std::string fr = std::string("\t\t required feature: ") + std::to_string(feat) + "\n";
				OutputDebugStringA(fr.c_str());
			} else {
				OutputDebugStringA("\t\t no required feature\n");
			}
			lang_sys_table::for_each_feature_index(d2, [](uint16_t id) {
				std::string f2 = "\t\t feature: " + std::to_string(id) + "\n";
				OutputDebugStringA(f2.c_str());
			});
		});
	});

	OutputDebugStringA("Feature List Table\n");
	auto fo = data + feature_offset(data);
	feature_list_table::for_each_feature_table(fo, [](opentype_tag const& t, uint8_t const* d, uint16_t i) {
		std::string tag = "\t[";
		tag += t.tag_data[0];
		tag += t.tag_data[1];
		tag += t.tag_data[2];
		tag += t.tag_data[3];
		tag += "] (";
		tag += std::to_string(i);
		tag += ") feature table\n";
		OutputDebugStringA(tag.c_str());

		feature_table::for_each_lookup_list_index(d, [](uint16_t feature_index) {
			std::string f2 = "\t\t lookup: " + std::to_string(feature_index) + "\n";
			OutputDebugStringA(f2.c_str());
		});
	});

	OutputDebugStringA("Lookup List Table\n");
	auto lo = data + lookup_offset(data);
	auto count = lookup_list_table::count(lo);
	auto count_str = std::string("\t total lookups: ") + std::to_string(count) + "\n";
	OutputDebugStringA(count_str.c_str());
	for(uint16_t i = 0; i < count; ++i) {
		auto nth = lookup_list_table::get_lookup_table(lo, i);
		assert(nth);
		auto type = lookup_table::get_lookup_type(nth);
		auto stc = lookup_table::get_sub_table_count(nth);
		auto tstr = std::string("\t type ") + std::to_string(type) + " lookup at position: " + std::to_string(i) + ", " + std::to_string(stc) + " sub tables\n";
		OutputDebugStringA(tstr.c_str());
	}
	OutputDebugStringA("DONE\n");
}
*/

} // end namespace gsub

namespace gdef {
uint8_t const* find(uint8_t const* file_data) {
	auto num_tables = b_to_u16(file_data + sizeof(uint32_t));
	auto table_records = file_data + sizeof(uint32_t) + sizeof(uint16_t) * 4;
	for(uint16_t i = 0; i < num_tables; ++i) {
		if(char(table_records[0]) == 'G' && char(table_records[1]) == 'D' && char(table_records[2]) == 'E' && char(table_records[3]) == 'F') {
			auto offset = b_to_u32(table_records + sizeof(uint32_t) * 2);
			return file_data + offset;
		}
		table_records += sizeof(uint32_t) * 4;
	}
	return nullptr;
}

uint8_t const* get_class_definition_table(uint8_t const* gd) {
	return gd + b_to_u16(gd + sizeof(uint16_t) + sizeof(uint16_t));
}
}

namespace class_definition_table{
uint16_t classify_glyph(uint8_t const* class_definition_table, uint16_t glyph) {
	if(!class_definition_table)
		return 0;

	auto format = b_to_u16(class_definition_table + 0);
	if(format == 1) {
		auto startGlyphID = b_to_u16(class_definition_table + sizeof(uint16_t));
		auto glyphCount = b_to_u16(class_definition_table + sizeof(uint16_t) + sizeof(uint16_t));
		if(glyph >= startGlyphID && glyph - startGlyphID < glyphCount) {
			return b_to_u16(class_definition_table + sizeof(uint16_t) * 3 + sizeof(uint16_t) * (glyph - startGlyphID));
		}
	} else if(format == 2) {
		auto classRangeCount = b_to_u16(class_definition_table + sizeof(uint16_t));
		auto rng = class_definition_table + sizeof(uint16_t) + sizeof(uint16_t);
		for(uint16_t i = 0; i < classRangeCount; ++i) {
			auto startGlyphID = b_to_u16(rng + 0);
			auto endGlyphID = b_to_u16(rng + sizeof(uint16_t));
			if(startGlyphID <= glyph && glyph <= endGlyphID) {
				return b_to_u16(rng + sizeof(uint16_t) + sizeof(uint16_t));
			}
			rng += sizeof(uint16_t) * 3;
		}
	}
	return 0;
}
}

namespace value_record {

inline constexpr uint16_t X_PLACEMENT = 0x0001;
inline constexpr uint16_t Y_PLACEMENT = 0x0002;
inline constexpr uint16_t X_ADVANCE = 0x0004;
inline constexpr uint16_t Y_ADVANCE = 0x0008;
inline constexpr uint16_t X_PLACEMENT_DEVICE = 0x0010;
inline constexpr uint16_t Y_PLACEMENT_DEVICE = 0x0020;
inline constexpr uint16_t X_ADVANCE_DEVICE = 0x0040;
inline constexpr uint16_t Y_ADVANCE_DEVICE = 0x0080;

size_t size(uint16_t format) {
	return size_t(std::popcount<uint16_t>(format) * 2);
}

int16_t get_x_placement(uint8_t const* t, uint16_t format) {
	if((format & X_PLACEMENT) == 0)
		return 0;
	return b_to_i16(t + 0);
}
int16_t get_y_placement(uint8_t const* t, uint16_t format) {
	if((format & Y_PLACEMENT) == 0)
		return 0;
	return b_to_i16(t + 2 * ((format & X_PLACEMENT) != 0));
}
int16_t get_x_advance(uint8_t const* t, uint16_t format) {
	if((format & X_ADVANCE) == 0)
		return 0;
	return b_to_i16(t + std::popcount<uint16_t>(format & (X_ADVANCE - 1)) * 2);
}
int16_t get_y_advance(uint8_t const* t, uint16_t format) {
	if((format & Y_ADVANCE) == 0)
		return 0;
	return b_to_i16(t + std::popcount<uint16_t>(format & (Y_ADVANCE - 1)) * 2);
}
uint8_t const* get_x_pla_device_table(uint8_t const* t, uint16_t format, uint8_t const* parent_base) {
	if((format & X_PLACEMENT_DEVICE) == 0)
		return nullptr;
	auto v = b_to_u16(t + std::popcount<uint16_t>(format & (X_PLACEMENT_DEVICE - 1)) * 2);
	return v ? parent_base + v : nullptr;
}
uint8_t const* get_y_pla_device_table(uint8_t const* t, uint16_t format, uint8_t const* parent_base) {
	if((format & Y_PLACEMENT_DEVICE) == 0)
		return nullptr;
	auto v = b_to_u16(t + std::popcount<uint16_t>(format & (Y_PLACEMENT_DEVICE - 1)) * 2);
	return v ? parent_base + v : nullptr;
}
uint8_t const* get_x_adv_device_table(uint8_t const* t, uint16_t format, uint8_t const* parent_base) {
	if((format & X_ADVANCE_DEVICE) == 0)
		return nullptr;
	auto v = b_to_u16(t + std::popcount<uint16_t>(format & (X_ADVANCE_DEVICE - 1)) * 2);
	return v ? parent_base + v : nullptr;
}
uint8_t const* get_y_adv_device_table(uint8_t const* t, uint16_t format, uint8_t const* parent_base) {
	if((format & Y_ADVANCE_DEVICE) == 0)
		return nullptr;
	auto v = b_to_u16(t + std::popcount<uint16_t>(format & (Y_ADVANCE_DEVICE - 1)) * 2);
	return v ? parent_base + v : nullptr;
}
}

namespace pair_value_record {
//constexpr inline size_t size = sizeof(uint16_t) + value_record::size * 2;
size_t size(uint16_t value_format_1, uint16_t value_format_2) {
	return sizeof(uint16_t) + value_record::size(value_format_1) + value_record::size(value_format_2);
}
}

namespace pair_set_table{
uint16_t get_pair_value_count(uint8_t const* t) {
	return b_to_u16(t + 0);
}
uint8_t const* get_pair_value_record(uint8_t const* t, uint16_t index, uint16_t value_format_1, uint16_t value_format_2) {
	auto first_offset = t + sizeof(uint16_t);
	return t + b_to_u16(first_offset + index * pair_value_record::size(value_format_1, value_format_2));
}
}

namespace pair_value_record {
uint16_t get_second_glyph(uint8_t const* t) {
	return b_to_u16(t + 0);
}
uint8_t const* get_value_record_1(uint8_t const* t, uint16_t value_format_1, uint16_t value_format_2) {
	return t + sizeof(uint16_t);
}
uint8_t const* get_value_record_2(uint8_t const* t, uint16_t value_format_1, uint16_t value_format_2) {
	return t + sizeof(uint16_t) + value_record::size(value_format_1);
}
}

namespace pair_pos_format_1 {
uint8_t const* get_coverage_table(uint8_t const* t) {
	return t + b_to_u16(t + sizeof(uint16_t));
}
uint16_t get_value_format_1(uint8_t const* t) {
	return b_to_u16(t + sizeof(uint16_t) + sizeof(uint16_t));
}
uint16_t get_value_format_2(uint8_t const* t) {
	return b_to_u16(t + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t));
}
uint16_t get_pair_set_count(uint8_t const* t) {
	return b_to_u16(t + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t));
}
uint8_t const* get_pair_set_table(uint8_t const* t, uint16_t index) {
	auto first_offset = t + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t);
	return t + b_to_u16(first_offset + index * sizeof(uint16_t));
}

int32_t net_x_adjust(uint8_t const* t, uint32_t glyph_index_a, uint32_t glyph_index_b) {
	auto ct = get_coverage_table(t);
	auto coverage_index = coverage_table::glyph_index(ct, uint16_t(glyph_index_a));
	if(coverage_index == -1)
		return 0;

	auto f1 = get_value_format_1(t);
	auto f2 = get_value_format_2(t);

	auto psc = get_pair_set_count(t);

	if(coverage_index >= psc)
		return 0;

	auto pst = get_pair_set_table(t, uint16_t(coverage_index));
	auto pstc = pair_set_table::get_pair_value_count(pst);
	for(uint16_t i = 0; i < pstc; ++i) {
		auto rec = pair_set_table::get_pair_value_record(pst, i, f1, f2);
		auto second = pair_value_record::get_second_glyph(rec);
		if(second == uint16_t(glyph_index_b)) {
			return value_record::get_x_advance(pair_value_record::get_value_record_1(rec, f1, f2), f1)
				+ value_record::get_x_placement(pair_value_record::get_value_record_2(rec, f1, f2), f2);
		}
	}
	return 0;
}
}


namespace class_2_record {
size_t size(uint16_t value_format_1, uint16_t value_format_2) {
	return value_record::size(value_format_1) + value_record::size(value_format_2);
}
uint8_t const* get_value_record_1(uint8_t const* t, uint16_t value_format_1, uint16_t value_format_2) {
	return t;
}
uint8_t const* get_value_record_2(uint8_t const* t, uint16_t value_format_1, uint16_t value_format_2) {
	return t  + value_record::size(value_format_1);
}
}

namespace pair_pos_format_2 {
uint8_t const* get_coverage_table(uint8_t const* t) {
	return t + b_to_u16(t + sizeof(uint16_t));
}
uint16_t get_value_format_1(uint8_t const* t) {
	return b_to_u16(t + sizeof(uint16_t) + sizeof(uint16_t));
}
uint16_t get_value_format_2(uint8_t const* t) {
	return b_to_u16(t + sizeof(uint16_t) + sizeof(uint16_t) + sizeof(uint16_t));
}
uint8_t const* get_class_def_1_table(uint8_t const* t) {
	return t + b_to_u16(t + sizeof(uint16_t) * 4);
}
uint8_t const* get_class_def_2_table(uint8_t const* t) {
	return t + b_to_u16(t + sizeof(uint16_t) * 5);
}
uint16_t get_class_1_count(uint8_t const* t) {
	return b_to_u16(t + sizeof(uint16_t) * 6);
}
uint16_t get_class_2_count(uint8_t const* t) {
	return b_to_u16(t + sizeof(uint16_t) * 7);
}
uint8_t const* get_class_2_record(uint8_t const* t,
	uint16_t class_1, uint16_t class_2,
	uint16_t class_1_count, uint16_t class_2_count,
	uint16_t value_format_1, uint16_t value_format_2) {

	if(class_1 >= class_1_count || class_2 >= class_2_count)
		return nullptr;

	auto base = t + sizeof(uint16_t) * 8;
	auto const c2rs = class_2_record::size(value_format_1, value_format_2);
	return base + class_1 * (class_2_count * c2rs) + class_2 * (c2rs);
}

int32_t net_x_adjust(uint8_t const* t, uint32_t glyph_index_a, uint32_t glyph_index_b) {
	auto ct = get_coverage_table(t);
	auto coverage_index = coverage_table::glyph_index(ct, uint16_t(glyph_index_a));
	if(coverage_index == -1)
		return 0;

	auto f1 = get_value_format_1(t);
	auto f2 = get_value_format_2(t);

	auto cdt1 = get_class_def_1_table(t);
	auto cdt2 = get_class_def_2_table(t);

	auto class_1 = class_definition_table::classify_glyph(cdt1, uint16_t(glyph_index_a));
	auto class_2 = class_definition_table::classify_glyph(cdt2, uint16_t(glyph_index_b));

	auto record = get_class_2_record(t, class_1, class_2, get_class_1_count(t), get_class_2_count(t), f1, f2);
	if(!record)
		return 0;

	return value_record::get_x_advance(class_2_record::get_value_record_1(record, f1, f2), f1)
		+ value_record::get_x_placement(class_2_record::get_value_record_2(record, f1, f2), f2);
}
}

namespace pair_pos_format {
int32_t net_x_adjust(uint8_t const* t, uint32_t glyph_index_a, uint32_t glyph_index_b) {
	auto format_type = b_to_u16(t);
	if(format_type == 1) {
		return pair_pos_format_1::net_x_adjust(t, glyph_index_a, glyph_index_b);
	} else if(format_type == 2) {
		return pair_pos_format_2::net_x_adjust(t, glyph_index_a, glyph_index_b);
	} else {
		return 0;
	}
}
}

namespace gpos {

uint8_t const* find(uint8_t const* file_data) {
	auto num_tables = b_to_u16(file_data + sizeof(uint32_t));
	auto table_records = file_data + sizeof(uint32_t) + sizeof(uint16_t) * 4;
	for(uint16_t i = 0; i < num_tables; ++i) {
		if(char(table_records[0]) == 'G' && char(table_records[1]) == 'P' && char(table_records[2]) == 'O' && char(table_records[3]) == 'S') {
			auto offset = b_to_u32(table_records + sizeof(uint32_t) * 2);
			return file_data + offset;
		}
		table_records += sizeof(uint32_t) * 4;
	}
	return nullptr;
}

/*
void dump_gpos(uint8_t const* data) {
	OutputDebugStringA("Script List Table\n");
	auto so = data + script_offset(data);
	script_list_table::for_each_script_table(so, [](opentype_tag const& t, uint8_t const* d) {
		std::string tag = "\t[";
		tag += t.tag_data[0];
		tag += t.tag_data[1];
		tag += t.tag_data[2];
		tag += t.tag_data[3];
		tag += "] script table\n";
		OutputDebugStringA(tag.c_str());

		auto def_table = script_table::default_lang_sys_table(d);
		if(def_table) {
			OutputDebugStringA("\t Default language system table:\n");
			auto feat = lang_sys_table::required_feature(def_table);
			if(feat != uint16_t(0xFFFF)) {
				std::string fr = std::string("\t\t required feature: ") + std::to_string(feat) + "\n";
				OutputDebugStringA(fr.c_str());
			} else {
				OutputDebugStringA("\t\t no required feature\n");
			}
			lang_sys_table::for_each_feature_index(def_table, [](uint16_t id) {
				std::string f2 = "\t\t feature: " + std::to_string(id) + "\n";
				OutputDebugStringA(f2.c_str());
			});
		} else {
			OutputDebugStringA("\t No default language system table\n");
		}

		script_table::for_each_lang_sys_table(d, [](opentype_tag const& t2, uint8_t const* d2) {
			std::string tag2 = "\t Language system table [";
			tag2 += t2.tag_data[0];
			tag2 += t2.tag_data[1];
			tag2 += t2.tag_data[2];
			tag2 += t2.tag_data[3];
			tag2 += "]\n";
			OutputDebugStringA(tag2.c_str());

			auto feat = lang_sys_table::required_feature(d2);
			if(feat != uint16_t(0xFFFF)) {
				std::string fr = std::string("\t\t required feature: ") + std::to_string(feat) + "\n";
				OutputDebugStringA(fr.c_str());
			} else {
				OutputDebugStringA("\t\t no required feature\n");
			}
			lang_sys_table::for_each_feature_index(d2, [](uint16_t id) {
				std::string f2 = "\t\t feature: " + std::to_string(id) + "\n";
				OutputDebugStringA(f2.c_str());
			});
		});
	});

	OutputDebugStringA("Feature List Table\n");
	auto fo = data + feature_offset(data);
	feature_list_table::for_each_feature_table(fo, [](opentype_tag const& t, uint8_t const* d, uint16_t i) {
		std::string tag = "\t[";
		tag += t.tag_data[0];
		tag += t.tag_data[1];
		tag += t.tag_data[2];
		tag += t.tag_data[3];
		tag += "] (";
		tag += std::to_string(i);
		tag += ") feature table\n";
		OutputDebugStringA(tag.c_str());

		feature_table::for_each_lookup_list_index(d, [](uint16_t feature_index) {
			std::string f2 = "\t\t lookup: " + std::to_string(feature_index) + "\n";
			OutputDebugStringA(f2.c_str());
		});
	});

	OutputDebugStringA("Lookup List Table\n");
	auto lo = data + lookup_offset(data);
	auto count = lookup_list_table::count(lo);
	auto count_str = std::string("\t total lookups: ") + std::to_string(count) + "\n";
	OutputDebugStringA(count_str.c_str());
	for(uint16_t i = 0; i < count; ++i) {
		auto nth = lookup_list_table::get_lookup_table(lo, i);
		assert(nth);
		auto type = lookup_table::get_lookup_type(nth);
		auto stc = lookup_table::get_sub_table_count(nth);
		auto tstr = std::string("\t type ") + std::to_string(type) + " lookup at position: " + std::to_string(i) + ", " + std::to_string(stc) + " sub tables\n";
		OutputDebugStringA(tstr.c_str());

		if(type == 8) { //Chained Contexts Positioning
			for(uint16_t j = 0; j < stc; ++j) {
				if(auto st = lookup_table::get_sub_table(nth, j); st) {
					auto fmt = coverage_table::get_format(st);
					auto fstr = std::string("\t\t format ") + std::to_string(fmt) + "\n";
					OutputDebugStringA(fstr.c_str());
				}
			}
		} else if(type == 9) { // Extension Positioning
			for(uint16_t j = 0; j < stc; ++j) {
				if(auto st = lookup_table::get_sub_table(nth, j); st) {
					auto fmt = coverage_table::get_format(st);
					auto ext_type = coverage_table::format1_glyph_count(st);

					auto fstr = std::string("\t\t format ") + std::to_string(fmt) + " as type " + std::to_string(ext_type) + "\n";
					OutputDebugStringA(fstr.c_str());
				}
			}
		} else if(type == 2) { // Pair Adjustment Positioning
			for(uint16_t j = 0; j < stc; ++j) {
				if(auto st = lookup_table::get_sub_table(nth, j); st) {
					auto fmt = coverage_table::get_format(st);
					auto fstr = std::string("\t\t format ") + std::to_string(fmt) + "\n";
					OutputDebugStringA(fstr.c_str());
				}
			}
		}
	}
	OutputDebugStringA("DONE\n");
}
*/

} // end GPOS

constexpr uint16_t pack_font_handle(uint32_t font_index, bool black, uint32_t size) {
	return uint16_t(uint32_t((font_index - 1) << 7) | uint32_t(black ? (1 << 6) : 0) | uint32_t(size & 0x3F));
}

bool is_black_font(std::string_view txt) {
	if(parsers::has_fixed_suffix_ci(txt.data(), txt.data() + txt.length(), "_bl") ||
			parsers::has_fixed_suffix_ci(txt.data(), txt.data() + txt.length(), "black") ||
			parsers::has_fixed_suffix_ci(txt.data(), txt.data() + txt.length(), "black_bold")) {
		return true;
	} else {
		return false;
	}
}

uint32_t font_size(std::string_view txt) {
	char const* first_int = txt.data();
	char const* end = txt.data() + txt.size();
	while(first_int != end && !isdigit(*first_int))
		++first_int;
	char const* last_int = first_int;
	while(last_int != end && isdigit(*last_int))
		++last_int;

	if(first_int == last_int) {
		if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "fps_font"))
			return uint32_t(14);
		else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "tooltip_font"))
			return uint32_t(16);
		else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "frangoth_bold"))
			return uint32_t(18);
		else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "impact_small"))
			return uint32_t(24);
		else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "old_english"))
			return uint32_t(50);
		else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "timefont"))
			return uint32_t(24);
		else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "vic_title"))
			return uint32_t(42);
		else
			return uint32_t(14);
	}

	uint32_t rvalue = 0;
	std::from_chars(first_int, last_int, rvalue);
	return rvalue;
}

uint32_t font_index(std::string_view txt) {
	if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "arial"))
		return 1;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "fps"))
		return 1;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "main"))
		return 2;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "tooltip"))
		return 1;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "frangoth"))
		return 2;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "garamond"))
		return 2;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "impact"))
		return 2;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "old"))
		return 2;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "timefont"))
		return 1;
	else if(parsers::has_fixed_prefix_ci(txt.data(), txt.data() + txt.size(), "vic"))
		return 2;
	else
		return 1;
}

uint16_t name_into_font_id(sys::state& state, std::string_view txt) {
	auto base_id = pack_font_handle(font_index(txt), is_black_font(txt), font_size(txt));
	std::string txt_copy = [&]() {
		if(parsers::has_fixed_suffix_ci(txt.data(), txt.data() + txt.length(), "_black")) {
			return std::string(txt.substr(0, txt.length() - 6));
		}
		if(parsers::has_fixed_suffix_ci(txt.data(), txt.data() + txt.length(), "_black_bold")) {
			return std::string(txt.substr(0, txt.length() - 11)) + "_bold";
		}
		return std::string(txt);
	}();
	uint16_t individuator = 0;
	auto it = state.font_collection.font_names.find(uint16_t(base_id | (individuator << 8)));
	while(it != state.font_collection.font_names.end()) {
		if(state.to_string_view(it->second) == txt_copy) {
			return uint16_t(base_id | (individuator << 8));
		}
		++individuator;
		it = state.font_collection.font_names.find(uint16_t(base_id | (individuator << 8)));
	}
	auto new_key = state.add_to_pool(txt_copy);
	auto new_handle = uint16_t(base_id | (individuator << 8));
	state.font_collection.font_names.insert_or_assign(new_handle, new_key);
	return new_handle;
}

int32_t size_from_font_id(uint16_t id) {
	auto index = uint32_t(((id >> 7) & 0x01) + 1);
	if(index == 2)
		return (int32_t(id & 0x3F) * 3) / 4;
	else
		return (int32_t(id & 0x3F) * 5) / 6;
}

bool is_black_from_font_id(uint16_t id) {
	return ((id >> 6) & 0x01) != 0;
}
uint32_t font_index_from_font_id(sys::state& state, uint16_t id) {
	uint32_t offset = 0;
	switch(state.languages[state.user_settings.current_language].script) {
	case text::language_script::chinese:
		offset += 3;
		break;
	case text::language_script::arabic:
		offset += 6;
		break;
	case text::language_script::cyrillic:
		offset += 9;
		break;
	default:
		break;
	}
	return uint32_t(((id >> 7) & 0x01) + 1) + offset;
}

font_manager::font_manager() {
	FT_Init_FreeType(&ft_library);
}
font_manager::~font_manager() {
	FT_Done_FreeType(ft_library);
}

font::~font() {
	if(hb_buf)
		hb_buffer_destroy(hb_buf);
	// if(loaded)
	//	FT_Done_Face(font_face);
}

int32_t transform_offset_b(int32_t x, int32_t y, int32_t btmap_x_off, int32_t btmap_y_off, uint32_t width, uint32_t height,
		uint32_t pitch) {
	int bmp_x = x - btmap_x_off;
	int bmp_y = y - btmap_y_off;

	if((bmp_x < 0) || (bmp_x >= (int32_t)width) || (bmp_y < 0) || (bmp_y >= (int32_t)height))
		return -1;
	else
		return bmp_x + bmp_y * (int32_t)pitch;
}

constexpr int magnification_factor = 4;
constexpr int dr_size = 64 * magnification_factor;
constexpr float rt_2 = 1.41421356237309504f;

void init_in_map(bool in_map[dr_size * dr_size], uint8_t const* bmp_data, int32_t btmap_x_off, int32_t btmap_y_off, uint32_t width, uint32_t height, uint32_t pitch) {
	for(int32_t j = 0; j < dr_size; ++j) {
		for(int32_t i = 0; i < dr_size; ++i) {
			auto const boff = transform_offset_b(i, j, btmap_x_off, btmap_y_off, width, height, pitch);
			in_map[i + dr_size * j] = (boff != -1) ? (bmp_data[boff] > 127) : false;
		}
	}
}

//
// based on The "dead reckoning" signed distance transform
// Grevera, George J. (2004) Computer Vision and Image Understanding 95 pages 317–333
//

void dead_reckoning(float distance_map[dr_size * dr_size], bool const in_map[dr_size * dr_size]) {
	int16_t yborder[dr_size * dr_size] = {0};
	int16_t xborder[dr_size * dr_size] = {0};

	for(uint32_t i = 0; i < dr_size * dr_size; ++i) {
		distance_map[i] = std::numeric_limits<float>::infinity();
	}
	for(int32_t j = 1; j < dr_size - 1; ++j) {
		for(int32_t i = 1; i < dr_size - 1; ++i) {
			if(in_map[i - 1 + dr_size * j] != in_map[i + dr_size * j] || in_map[i + 1 + dr_size * j] != in_map[i + dr_size * j] ||
					in_map[i + dr_size * (j + 1)] != in_map[i + dr_size * j] || in_map[i + dr_size * (j - 1)] != in_map[i + dr_size * j]) {
				distance_map[i + dr_size * j] = 0.0f;
				yborder[i + dr_size * j] = static_cast<int16_t>(j);
				xborder[i + dr_size * j] = static_cast<int16_t>(i);
			}
		}
	}
	for(int32_t j = 1; j < dr_size - 1; ++j) {
		for(int32_t i = 1; i < dr_size - 1; ++i) {
			if(distance_map[(i - 1) + dr_size * (j - 1)] + rt_2 < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i - 1) + dr_size * (j - 1)];
				xborder[i + dr_size * j] = xborder[(i - 1) + dr_size * (j - 1)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
			if(distance_map[(i) + dr_size * (j - 1)] + 1.0f < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i) + dr_size * (j - 1)];
				xborder[i + dr_size * j] = xborder[(i) + dr_size * (j - 1)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
			if(distance_map[(i + 1) + dr_size * (j - 1)] + rt_2 < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i + 1) + dr_size * (j - 1)];
				xborder[i + dr_size * j] = xborder[(i + 1) + dr_size * (j - 1)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
			if(distance_map[(i - 1) + dr_size * (j)] + 1.0f < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i - 1) + dr_size * (j)];
				xborder[i + dr_size * j] = xborder[(i - 1) + dr_size * (j)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
		}
	}

	for(int32_t j = dr_size - 2; j > 0; --j) {
		for(int32_t i = dr_size - 2; i > 0; --i) {
			if(distance_map[(i + 1) + dr_size * (j)] + 1.0f < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i + 1) + dr_size * (j)];
				xborder[i + dr_size * j] = xborder[(i + 1) + dr_size * (j)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
			if(distance_map[(i - 1) + dr_size * (j + 1)] + rt_2 < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i - 1) + dr_size * (j + 1)];
				xborder[i + dr_size * j] = xborder[(i - 1) + dr_size * (j + 1)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
			if(distance_map[(i) + dr_size * (j + 1)] + 1.0f < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i) + dr_size * (j + 1)];
				xborder[i + dr_size * j] = xborder[(i) + dr_size * (j + 1)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
			if(distance_map[(i + 1) + dr_size * (j + 1)] + rt_2 < distance_map[(i) + dr_size * (j)]) {
				yborder[i + dr_size * j] = yborder[(i + 1) + dr_size * (j + 1)];
				xborder[i + dr_size * j] = xborder[(i + 1) + dr_size * (j + 1)];
				distance_map[(i) + dr_size * (j)] = (float)std::sqrt((i - xborder[i + dr_size * j]) * (i - xborder[i + dr_size * j]) +
																														 (j - yborder[i + dr_size * j]) * (j - yborder[i + dr_size * j]));
			}
		}
	}

	for(uint32_t i = 0; i < dr_size * dr_size; ++i) {
		if(in_map[i])
			distance_map[i] *= -1.0f;
	}
}

void font_manager::load_font(font& fnt, char const* file_data, uint32_t file_size, font_feature f) {
	fnt.file_data = std::unique_ptr<FT_Byte[]>(new FT_Byte[file_size]);
	fnt.features = f;
	memcpy(fnt.file_data.get(), file_data, file_size);
	FT_New_Memory_Face(ft_library, fnt.file_data.get(), file_size, 0, &fnt.font_face);
	FT_Select_Charmap(fnt.font_face, FT_ENCODING_UNICODE);
	FT_Set_Pixel_Sizes(fnt.font_face, 0, dr_size);
	fnt.hb_font_face = hb_ft_font_create(fnt.font_face, nullptr);
	hb_font_set_scale(fnt.hb_font_face, dr_size, dr_size);
	fnt.hb_buf = hb_buffer_create();
	if(fnt.features == text::font_feature::small_caps) {
		fnt.hb_features[0].tag = hb_tag_from_string("smcp", 4);
		fnt.hb_features[0].start = 0; /* Start point in text */
		fnt.hb_features[0].end = (unsigned int)-1; /* End point in text */
		fnt.hb_features[0].value = 1;
		fnt.num_features = 1;
	}
	fnt.loaded = true;

	fnt.internal_line_height = float(fnt.font_face->size->metrics.height) / float((1 << 6) * magnification_factor);
	fnt.internal_ascender = float(fnt.font_face->size->metrics.ascender) / float((1 << 6) * magnification_factor);
	fnt.internal_descender = -float(fnt.font_face->size->metrics.descender) / float((1 << 6) * magnification_factor);
	fnt.internal_top_adj = (fnt.internal_line_height - (fnt.internal_ascender + fnt.internal_descender)) / 2.0f;
}

float font::line_height(int32_t size) const {
	return internal_line_height * size / 64.0f;
}
float font::ascender(int32_t size) const {
	return internal_ascender * size / 64.0f;
}
float font::descender(int32_t size) const {
	return internal_descender * size / 64.0f;
}
float font::top_adjustment(int32_t size) const {
	return internal_top_adj * size / 64.0f;
}

float font_manager::line_height(sys::state& state, uint16_t font_id) const {
	if(state.user_settings.use_classic_fonts) {
		return text::get_bm_font(state, font_id).get_height();
	} else {
		return float(fonts[text::font_index_from_font_id(state, font_id) - 1].line_height(text::size_from_font_id(font_id)));
	}
}
float font_manager::text_extent(sys::state& state, char const* codepoints, uint32_t count, uint16_t font_id) {
	if(state.user_settings.use_classic_fonts) {
		return text::get_bm_font(state, font_id).get_string_width(state, codepoints, count);
	} else {
		return float(fonts[text::font_index_from_font_id(state, font_id) - 1].text_extent(state, codepoints, count, text::size_from_font_id(font_id)));
	}
}

bool font::can_display(char32_t ch_in) const {
	return FT_Get_Char_Index(font_face, ch_in) != 0;
}

std::string font::get_conditional_indicator(bool v) const {
	if(v) {
		if(can_display(U'✔')) {
			return "✔";
		}
		if(can_display(U'✓')) {
			return "✓";
		}
		return "@(T)";
	} else {
		if(can_display(U'✘')) {
			return "✘";
		}
		if(can_display(U'✗')) {
			return "✗";
		}
		return "@(F)";
	}
}

void font::make_glyph(char32_t ch_in) {
	if(glyph_loaded.find(ch_in) != glyph_loaded.end())
		return;
	glyph_loaded.insert_or_assign(ch_in, true);
	auto index_in_this_font = ch_in;
	// load all glyph metrics
	if(index_in_this_font) {
		FT_Load_Glyph(font_face, index_in_this_font, FT_LOAD_TARGET_NORMAL | FT_LOAD_RENDER);

		FT_Glyph g_result;
		FT_Get_Glyph(font_face->glyph, &g_result);

		FT_Bitmap const& bitmap = ((FT_BitmapGlyphRec*)g_result)->bitmap;

		float const hb_x = float(font_face->glyph->metrics.horiBearingX) / 64.f;
		float const hb_y = float(font_face->glyph->metrics.horiBearingY) / 64.f;

		auto sub_index = ch_in & 63;
		uint8_t pixel_buffer[64 * 64] = { 0 };
		int const btmap_x_off = 32 * magnification_factor - bitmap.width / 2;
		int const btmap_y_off = 32 * magnification_factor - bitmap.rows / 2;

		glyph_sub_offset gso;
		gso.x = (hb_x - float(btmap_x_off)) * 1.0f / float(magnification_factor);
		gso.y = (-hb_y - float(btmap_y_off)) * 1.0f / float(magnification_factor);
		glyph_positions.insert_or_assign(index_in_this_font, gso);
		glyph_advances.insert_or_assign(index_in_this_font, float(font_face->glyph->metrics.horiAdvance) / float((1 << 6) * magnification_factor));

		bool in_map[dr_size * dr_size] = {false};
		float distance_map[dr_size * dr_size] = {0.0f};
		init_in_map(in_map, bitmap.buffer, btmap_x_off, btmap_y_off, bitmap.width, bitmap.rows, uint32_t(bitmap.pitch));
		dead_reckoning(distance_map, in_map);
		for(int y = 0; y < 64; ++y) {
			for(int x = 0; x < 64; ++x) {
				const size_t index = size_t(x + y * 64);
				float const distance_value = distance_map[(x * magnification_factor + magnification_factor / 2) + (y * magnification_factor + magnification_factor / 2) * dr_size] / float(magnification_factor * 64);
				int const int_value = int(distance_value * -255.0f + 128.0f);
				const uint8_t small_value = uint8_t(std::min(255, std::max(0, int_value)));
				pixel_buffer[index] = small_value;
			}
		}
		//The array
		auto texture_number = (ch_in >> 6) % text::max_texture_layers;
		if(texture_array == 0) {
			glGenTextures(1, &texture_array);
			glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);
			glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_R8, 64 * 8, 64 * 8, GLsizei(text::max_texture_layers));
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		} else {
			glBindTexture(GL_TEXTURE_2D_ARRAY, texture_array);
		}
		if(texture_array) {
			glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, (sub_index & 7) * 64, ((sub_index >> 3) & 7) * 64, GLint(texture_number), 64, 64, 1, GL_RED, GL_UNSIGNED_BYTE, pixel_buffer);
		}
		FT_Done_Glyph(g_result);
	}
}

char font::codepoint_to_alnum(char32_t codepoint) {
	std::string_view alnum_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789()";
	for(const auto c : alnum_table)
		if(codepoint == FT_Get_Char_Index(font_face, c))
			return c;
	return 0;
}

decltype(font::cached_text)::iterator font::get_cached_glyphs(char const* codepoints, uint32_t count) {
	auto s = std::string(std::string_view(codepoints, codepoints + count));
	if(auto it = cached_text.find(s); it != cached_text.end()) {
		return it;
	} else {
		hb_buffer_clear_contents(hb_buf);
		hb_buffer_add_utf8(hb_buf, codepoints, int(count), 0, -1);
		hb_buffer_guess_segment_properties(hb_buf);
		hb_shape(hb_font_face, hb_buf, hb_features, num_features);
		unsigned int glyph_count;
		hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(hb_buf, &glyph_count);
		hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(hb_buf, &glyph_count);
		for(unsigned int i = 0; i < glyph_count; i++) { // Preload glyphs
			make_glyph(glyph_info[i].codepoint);
		}
		text::cached_text_entry entry{};
		entry.glyph_info.resize(size_t(glyph_count));
		std::memcpy(entry.glyph_info.data(), glyph_info, glyph_count * sizeof(glyph_info[0]));
		entry.glyph_pos.resize(size_t(glyph_count));
		std::memcpy(entry.glyph_pos.data(), glyph_pos, glyph_count * sizeof(glyph_pos[0]));
		cached_text.insert_or_assign(s, entry);
		return cached_text.find(s);
	}
}

float font::text_extent(sys::state& state, char const* codepoints, uint32_t count, int32_t size) {
	auto it = get_cached_glyphs(codepoints, count);
	assert(it != cached_text.end());
	hb_glyph_position_t* glyph_pos = it->second.glyph_pos.data();
	hb_glyph_info_t* glyph_info = it->second.glyph_info.data();
	unsigned int glyph_count = static_cast<unsigned int>(it->second.glyph_info.size());
	//
	float total = 0.0f;
	for(unsigned int i = 0; i < glyph_count; i++) {
		hb_codepoint_t glyphid = glyph_info[i].codepoint;
		auto gso = glyph_positions[glyphid];
		float x_advance = float(glyph_advances[glyphid]);
		bool draw_icon = false;
		bool draw_flag = false;
		if(glyphid == FT_Get_Char_Index(font_face, '@')) {
			char tag[3] = { 0, 0, 0 };
			tag[0] = (i + 1 < glyph_count) ? codepoint_to_alnum(glyph_info[i + 1].codepoint) : 0;
			tag[1] = (i + 2 < glyph_count) ? codepoint_to_alnum(glyph_info[i + 2].codepoint) : 0;
			tag[2] = (i + 3 < glyph_count) ? codepoint_to_alnum(glyph_info[i + 3].codepoint) : 0;
			if(tag[0] == '(' && tag[2] == ')') {
				if(tag[1] == 'F' || tag[1] == 'T') { //(F)alse or (T)rue
					draw_icon = true;
				} else { //(A)rmy or (N)avy
					draw_icon = true;
				}
			} else if(tag[0] != 0 && tag[1] != 0 && tag[2] != 0) {
				draw_icon = true;
				draw_flag = true;
			}
			if(draw_icon) {
				i += 3;
			}
		}
		total += x_advance * (draw_flag ? 1.5f : 1.f) * size / 64.f;
	}
	return total;
}

} // namespace text

#include "parsers.hpp"
struct font_body {
	uint32_t id = 0;
	std::vector<std::string> valid_paths;
	bool smallcaps = false;
	void path(parsers::association_type, std::string_view value, parsers::error_handler& err, int32_t line, parsers::scenario_building_context& context) {
		if(valid_paths.empty()) {
			valid_paths.push_back(std::string(value));
		} else {
			valid_paths.resize(valid_paths.size() + 1);
			valid_paths.back() = valid_paths.front();
			valid_paths.front() = std::string(value);
		}
	}
	void fallback(parsers::association_type, std::string_view value, parsers::error_handler& err, int32_t line, parsers::scenario_building_context& context) {
		valid_paths.push_back(std::string(value));
	}
	void finish(parsers::scenario_building_context& context) { }
};
struct font_file {
	bool blackmapfont = true;
	void font(font_body value, parsers::error_handler& err, int32_t line, parsers::scenario_building_context& context) {
		auto root = simple_fs::get_root(context.state.common_fs);
		for(const auto& path : value.valid_paths) {
			if(auto f = simple_fs::open_file(root, simple_fs::utf8_to_native(path)); f) {
				auto file_content = simple_fs::view_contents(*f);
				auto feature = text::font_feature::none;
				if(value.smallcaps)
					feature = text::font_feature::small_caps;
				context.state.font_collection.load_font(context.state.font_collection.fonts[value.id - 1], file_content.data, file_content.file_size, feature);
				break;
			}
		}
	}
	void finish(parsers::scenario_building_context& context) { }
};
#include "font_defs_generated.hpp"

namespace text {

void load_standard_fonts(sys::state& state) {
	auto root = get_root(state.common_fs);
	if(auto f = open_file(root, NATIVE("assets/fonts/font.txt")); f) {
		auto content = view_contents(*f);
		parsers::error_handler err("");
		parsers::scenario_building_context context(state);
		err.file_name = simple_fs::native_to_utf8(simple_fs::get_full_name(*f));
		parsers::token_generator gen(content.data, content.data + content.file_size);
		auto font = parse_font_file(gen, err, context);
		state.font_collection.map_font_is_black = font.blackmapfont;
	}
}

void load_bmfonts(sys::state& state) { }

void font_manager::load_all_glyphs() {
	//for(uint32_t j = 0; j < std::extent_v<decltype(fonts)>; ++j) {
	//	for(uint32_t i = 0; i < 256; ++i) {
	//		auto codepoint = char32_t(win1250toUTF16(char(i)));
	//		auto index = FT_Get_Char_Index(fonts[j].font_face, codepoint);
	//		fonts[j].make_glyph(index);
	//	}
	//}
}

} // namespace text
