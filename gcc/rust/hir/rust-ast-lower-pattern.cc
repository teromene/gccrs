// Copyright (C) 2020-2022 Free Software Foundation, Inc.

// This file is part of GCC.

// GCC is free software; you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 3, or (at your option) any later
// version.

// GCC is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
// for more details.

// You should have received a copy of the GNU General Public License
// along with GCC; see the file COPYING3.  If not see
// <http://www.gnu.org/licenses/>.

#include "rust-ast-lower-pattern.h"
#include "rust-ast-lower-expr.h"

namespace Rust {
namespace HIR {

ASTLoweringPattern::ASTLoweringPattern () : translated (nullptr) {}

HIR::Pattern *
ASTLoweringPattern::translate (AST::Pattern *pattern)
{
  ASTLoweringPattern resolver;
  pattern->accept_vis (resolver);

  rust_assert (resolver.translated != nullptr);

  resolver.mappings->insert_hir_pattern (resolver.translated);
  resolver.mappings->insert_location (
    resolver.translated->get_pattern_mappings ().get_hirid (),
    pattern->get_locus ());

  return resolver.translated;
}

void
ASTLoweringPattern::visit (AST::IdentifierPattern &pattern)
{
  auto crate_num = mappings->get_current_crate ();
  Analysis::NodeMapping mapping (crate_num, pattern.get_node_id (),
				 mappings->get_next_hir_id (crate_num),
				 UNKNOWN_LOCAL_DEFID);

  std::unique_ptr<Pattern> to_bind;
  translated
    = new HIR::IdentifierPattern (mapping, pattern.get_ident (),
				  pattern.get_locus (), pattern.get_is_ref (),
				  pattern.get_is_mut () ? Mutability::Mut
							: Mutability::Imm,
				  std::move (to_bind));
}

void
ASTLoweringPattern::visit (AST::PathInExpression &pattern)
{
  translated = ASTLowerPathInExpression::translate (&pattern);
}

void
ASTLoweringPattern::visit (AST::TupleStructPattern &pattern)
{
  HIR::PathInExpression *path
    = ASTLowerPathInExpression::translate (&pattern.get_path ());

  TupleStructItems *lowered = nullptr;
  auto &items = pattern.get_items ();
  switch (items->get_item_type ())
    {
      case AST::TupleStructItems::RANGE: {
	// TODO
	gcc_unreachable ();
      }
      break;

      case AST::TupleStructItems::NO_RANGE: {
	AST::TupleStructItemsNoRange &items_no_range
	  = static_cast<AST::TupleStructItemsNoRange &> (*items.get ());

	std::vector<std::unique_ptr<HIR::Pattern> > patterns;
	for (auto &inner_pattern : items_no_range.get_patterns ())
	  {
	    HIR::Pattern *p
	      = ASTLoweringPattern::translate (inner_pattern.get ());
	    patterns.push_back (std::unique_ptr<HIR::Pattern> (p));
	  }

	lowered = new HIR::TupleStructItemsNoRange (std::move (patterns));
      }
      break;
    }

  auto crate_num = mappings->get_current_crate ();
  Analysis::NodeMapping mapping (crate_num, pattern.get_node_id (),
				 mappings->get_next_hir_id (crate_num),
				 UNKNOWN_LOCAL_DEFID);

  translated = new HIR::TupleStructPattern (
    mapping, *path, std::unique_ptr<HIR::TupleStructItems> (lowered));
}

void
ASTLoweringPattern::visit (AST::StructPattern &pattern)
{
  HIR::PathInExpression *path
    = ASTLowerPathInExpression::translate (&pattern.get_path ());

  auto &raw_elems = pattern.get_struct_pattern_elems ();
  rust_assert (!raw_elems.has_etc ());

  std::vector<std::unique_ptr<HIR::StructPatternField> > fields;
  for (auto &field : raw_elems.get_struct_pattern_fields ())
    {
      HIR::StructPatternField *f = nullptr;
      switch (field->get_item_type ())
	{
	  case AST::StructPatternField::ItemType::TUPLE_PAT: {
	    // TODO
	    gcc_unreachable ();
	  }
	  break;

	  case AST::StructPatternField::ItemType::IDENT_PAT: {
	    // TODO
	    gcc_unreachable ();
	  }
	  break;

	  case AST::StructPatternField::ItemType::IDENT: {
	    AST::StructPatternFieldIdent &ident
	      = static_cast<AST::StructPatternFieldIdent &> (*field.get ());

	    auto crate_num = mappings->get_current_crate ();
	    Analysis::NodeMapping mapping (crate_num, ident.get_node_id (),
					   mappings->get_next_hir_id (
					     crate_num),
					   UNKNOWN_LOCAL_DEFID);

	    f = new HIR::StructPatternFieldIdent (
	      mapping, ident.get_identifier (), ident.is_ref (),
	      ident.is_mut () ? Mutability::Mut : Mutability::Imm,
	      ident.get_outer_attrs (), ident.get_locus ());
	  }
	  break;
	}

      // insert the reverse mappings and locations
      auto field_id = f->get_mappings ().get_hirid ();
      auto field_node_id = f->get_mappings ().get_nodeid ();
      mappings->insert_location (field_id, f->get_locus ());
      mappings->insert_node_to_hir (field_node_id, field_id);

      // add it to the lowered fields list
      fields.push_back (std::unique_ptr<HIR::StructPatternField> (f));
    }

  auto crate_num = mappings->get_current_crate ();
  Analysis::NodeMapping mapping (crate_num, pattern.get_node_id (),
				 mappings->get_next_hir_id (crate_num),
				 UNKNOWN_LOCAL_DEFID);

  HIR::StructPatternElements elems (std::move (fields));
  translated = new HIR::StructPattern (mapping, *path, std::move (elems));
}

void
ASTLoweringPattern::visit (AST::WildcardPattern &pattern)
{
  auto crate_num = mappings->get_current_crate ();
  Analysis::NodeMapping mapping (crate_num, pattern.get_node_id (),
				 mappings->get_next_hir_id (crate_num),
				 UNKNOWN_LOCAL_DEFID);

  translated = new HIR::WildcardPattern (mapping, pattern.get_locus ());
}

void
ASTLoweringPattern::visit (AST::TuplePattern &pattern)
{
  std::unique_ptr<HIR::TuplePatternItems> items;
  switch (pattern.get_items ()->get_pattern_type ())
    {
      case AST::TuplePatternItems::TuplePatternItemType::MULTIPLE: {
	AST::TuplePatternItemsMultiple &ref
	  = *static_cast<AST::TuplePatternItemsMultiple *> (
	    pattern.get_items ().get ());
	items = lower_tuple_pattern_multiple (ref);
      }
      break;

      case AST::TuplePatternItems::TuplePatternItemType::RANGED: {
	AST::TuplePatternItemsRanged &ref
	  = *static_cast<AST::TuplePatternItemsRanged *> (
	    pattern.get_items ().get ());
	items = lower_tuple_pattern_ranged (ref);
      }
      break;
    }

  auto crate_num = mappings->get_current_crate ();
  Analysis::NodeMapping mapping (crate_num, pattern.get_node_id (),
				 mappings->get_next_hir_id (crate_num),
				 UNKNOWN_LOCAL_DEFID);

  translated
    = new HIR::TuplePattern (mapping, std::move (items), pattern.get_locus ());
}

void
ASTLoweringPattern::visit (AST::LiteralPattern &pattern)
{
  auto crate_num = mappings->get_current_crate ();
  Analysis::NodeMapping mapping (crate_num, pattern.get_node_id (),
				 mappings->get_next_hir_id (crate_num),
				 UNKNOWN_LOCAL_DEFID);

  HIR::Literal l = lower_literal (pattern.get_literal ());
  translated
    = new HIR::LiteralPattern (mapping, std::move (l), pattern.get_locus ());
}

void
ASTLoweringPattern::visit (AST::RangePattern &pattern)
{
  auto upper_bound
    = lower_range_pattern_bound (pattern.get_upper_bound ().get ());
  auto lower_bound
    = lower_range_pattern_bound (pattern.get_lower_bound ().get ());

  auto crate_num = mappings->get_current_crate ();
  Analysis::NodeMapping mapping (crate_num, pattern.get_node_id (),
				 mappings->get_next_hir_id (crate_num),
				 UNKNOWN_LOCAL_DEFID);

  translated
    = new HIR::RangePattern (mapping, std::move (lower_bound),
			     std::move (upper_bound), pattern.get_locus ());
}

} // namespace HIR
} // namespace Rust
