#include "ontology/onto_sexpr.h"
#include "ontology/onto_keywords.h"
#include "seq/pkeywords.h"
#include "guile/newick-type.h"

Terminatrix::Terminatrix()
  : var_counts(pscores),
    dummy_alph(),
    matrix_set(dummy_alph),
    got_counts(false)
{ }

EM_matrix_base& Terminatrix::rate_matrix()
{
  if (matrix_set.chain.size() != 1 || matrix_set.chain[0].matrix == NULL)
    THROWEXPR ("Oops -- no rate matrix in Terminatrix");
  return *matrix_set.chain[0].matrix;
}

ECFG_chain& Terminatrix::chain()
{
  if (matrix_set.chain.size() != 1)
    THROWEXPR ("Oops -- no chain in Terminatrix");
  return matrix_set.chain[0];
}

const Alphabet& Terminatrix::default_alphabet()
{
  if (alph_list.empty())
    THROWEXPR ("Oops -- no alphabet in Terminatrix");
  return alph_list.front();
}

void Terminatrix::eval_funcs()
{
  matrix_set.eval_funcs (pscores);
}

void Terminatrix_builder::init_terminatrix (Terminatrix& term, SExpr& wrapper_sexpr)
{
  SExpr& terminatrix_sexpr = wrapper_sexpr.find_or_die (TERMINATRIX_TERMINATRIX);
  const Ass_map ass_map (terminatrix_sexpr, 1);
  init_terminatrix (term, ass_map);
}

void Terminatrix_builder::init_terminatrix (Terminatrix& term, SCM args_scm)
{
  const Ass_map ass_map (args_scm);
  init_terminatrix (term, ass_map);
}

void Terminatrix_builder::init_terminatrix (Terminatrix& term, const Ass_map& ass_map)
{
  init_terminatrix_member_scm (term.init_scm, ass_map, TERMINATRIX_INIT_SCM);
  init_terminatrix_member_scm (term.model_scm, ass_map, TERMINATRIX_MODEL_SCM);
  init_terminatrix_member_scm (term.tree_db_scm, ass_map, TERMINATRIX_TREE_DB_SCM);
  init_terminatrix_member_scm (term.knowledge_scm, ass_map, TERMINATRIX_KNOWLEDGE_SCM);

  if (!scm_is_false (term.init_scm))
    (void) scm_primitive_eval (term.init_scm);

  SExpr* params_sexpr = ass_map.new_sexpr_value_or_null (TERMINATRIX_PARAMS);
  if (params_sexpr)
    {
      init_terminatrix_params (term, *params_sexpr);
      delete params_sexpr;
    }

  SExpr model_sexpr;
  if (!scm_is_false (term.model_scm))
    {
      // if model is generated by Scheme, ensure it does not encode parameter values
      model_sexpr = scm_to_sexpr (scm_primitive_eval (term.model_scm));
      init_terminatrix_model (term, model_sexpr);
      if (term.chain().type != Parametric)
	THROWEXPR(TERMINATRIX_MODEL_SCM << " function must return (" << TERMINATRIX_MODEL << " (" << EG_CHAIN << " (" << EG_CHAIN_POLICY << " " << EG_PARAMETRIC << ") ...) ...)");
    }
  else
    {
      model_sexpr = ass_map.sexpr_value_or_die (TERMINATRIX_MODEL);
      init_terminatrix_model (term, model_sexpr);
    }
}

void Terminatrix_builder::init_terminatrix_member_scm (SCM& member_scm, const Ass_map& parent_ass_map, const char* tag)
{
  member_scm = parent_ass_map.scm_value_or_false (tag);
}

void Terminatrix_builder::init_terminatrix_params (Terminatrix& term, SExpr& model_sexpr)
{
  // initialise PScores
  init_pgroups_and_rates (term.pscores, term.sym2pvar, model_sexpr, &term.mutable_pgroups);
  // initialise PCounts
  init_pseudocounts (term.pcounts, term.pscores, term.sym2pvar, model_sexpr);
  term.var_counts = PCounts (term.pscores);
}

void Terminatrix_builder::init_terminatrix_model (Terminatrix& term, SExpr& model_sexpr)
{
  // initialise alphabet(s)
  const vector<SExpr*> alph_sexprs = model_sexpr.find_all (PK_ALPHABET);
  if (alph_sexprs.size() == 0)
    THROWEXPR ("You need at least one alphabet");
  for_const_contents (vector<SExpr*>, alph_sexprs, alph_sexpr)
    {
      term.alph_list.push_back (term.dummy_alph);
      Alphabet& alph (term.alph_list.back());
      init_alphabet (alph, **alph_sexpr, true);
      term.alph_dict.add (alph);
    }
  // initialise chain
  init_chain (term.matrix_set, term.alph_dict, term.default_alphabet(), term.term2chain, term.sym2pvar, model_sexpr.find_or_die (EG_CHAIN), DEFAULT_TIMEPOINT_RES, true);
}

void Terminatrix_builder::terminatrix2stream (ostream& out, Terminatrix& term)
{
  out << '(' << TERMINATRIX_TERMINATRIX << '\n';
  terminatrix_member_scm2stream (out, term.init_scm, TERMINATRIX_INIT_SCM);

  if (!scm_is_false (term.model_scm))
    terminatrix_member_scm2stream (out, term.model_scm, TERMINATRIX_MODEL_SCM);
  else
    terminatrix_model2stream (out, term);

  terminatrix_member_scm2stream (out, term.tree_db_scm, TERMINATRIX_TREE_DB_SCM);
  terminatrix_member_scm2stream (out, term.knowledge_scm, TERMINATRIX_KNOWLEDGE_SCM);

  terminatrix_params2stream (out, term);

  out << ") ;; end " TERMINATRIX_TERMINATRIX "\n";
}

void Terminatrix_builder::terminatrix_member_scm2stream (ostream& out, SCM& member_scm, const char* tag)
{
  if (!scm_is_false (member_scm))
    {
      sstring member_scm_str = scm_to_string (member_scm);
      out << " (" << tag << " " << member_scm_str << ")\n";
    }
}

void Terminatrix_builder::terminatrix_model2stream (ostream& out, Terminatrix& term)
{
  out << " (" << TERMINATRIX_MODEL << '\n';
  chain2stream (out, term.pscores, ((Terminatrix&)term).chain(), term.alph_dict, term.default_alphabet(), term.got_counts ? &term.stats : NULL);
  for_const_contents (list<Alphabet>, term.alph_list, alph)
    alphabet2stream (out, *alph);
  out << " ) ;; end " TERMINATRIX_MODEL "\n";
}

void Terminatrix_builder::terminatrix_params2stream (ostream& out, Terminatrix& term)
{
  out << "\n (" << TERMINATRIX_PARAMS << '\n';
  pscores2stream (out, term.pscores, term.mutable_pgroups, &term.var_counts);
  pcounts2stream (out, term.pcounts, EG_PSEUDOCOUNTS, (const PCounts*) 0, true, false);
  if (term.got_counts)
    pcounts2stream (out, term.var_counts, EG_EXPECTED_COUNTS, &term.pcounts, true, true);
  out << " ) ;; end " TERMINATRIX_PARAMS "\n";
}

SCM Terminatrix_family_visitor::map_reduce_scm()
{
  SCM tree_db_list_scm = scm_primitive_eval (terminatrix.tree_db_scm);
  if (!scm_list_p(tree_db_list_scm))
    THROWEXPR (TERMINATRIX_TREE_DB_SCM " function must return a list");

  CTAG(5,TERMINATRIX) << "Beginning iteration over families\n";
  scm_t_bits accumulator = zero();

  current_family_index = 0;
  while (!scm_is_null(tree_db_list_scm))
    {
      CTAG(5,TERMINATRIX) << "Visiting family #" << (current_family_index + 1) << "\n";

      SCM tree_db_entry_scm = scm_car (tree_db_list_scm);
      if (!(scm_list_p(tree_db_entry_scm) && scm_to_uintmax(scm_length(tree_db_entry_scm)) == 2))
	THROWEXPR (TERMINATRIX_TREE_DB_SCM " function must return a list");

      current_name_scm = scm_car (tree_db_entry_scm);
      current_newick_scm = scm_cadr (tree_db_entry_scm);

      if (!scm_is_string(current_name_scm))
	CLOGERR << TERMINATRIX_TREE_DB_SCM " function must return a list of (name newick) lists, where the name is a string\n";

      current_name = scm_to_string (current_name_scm);
      current_tree = newick_cast_from_scm (current_newick_scm);

      CTAG(5,TERMINATRIX) << "Family #" << (current_family_index + 1) << ": " << current_name << "\n";

      init_current();
      accumulator = reduce (accumulator);

      tree_db_list_scm = scm_cdr (tree_db_list_scm);
      ++current_family_index;
    }

  CTAG(5,TERMINATRIX) << "All families done; finalizing...\n";
  return finalize (accumulator);
}

void Terminatrix_EM_visitor::initialize_current_colmat()
{
  map<sstring,Symbol_score_map> named_node_scores;
  // fill named_node_scores by calling knowledge function
  SCM knowledge_func_scm = scm_primitive_eval (terminatrix.knowledge_scm);
  for (Phylogeny::Node n = 0; n < current_tree->nodes(); ++n)
    if (current_tree->node_name[n].size())
    {
      const sstring& node_name = current_tree->node_name[n];
      SCM node_name_scm = scm_from_locale_string (node_name.c_str());
      Symbol_score_map node_ssm;
      // loop over states, creating state tuples and calling knowledge function as follows:
      // (knowledge familyName geneName (term1 term2 ... hiddenState))
      for (int state = 0; state < terminatrix.rate_matrix().number_of_states(); ++state)
	{
	  const vector<sstring> state_tokens = terminatrix.chain().get_symbol_tokens (state, terminatrix.alph_dict, terminatrix.default_alphabet());

	  SCM state_tuple_scm = scm_list_n (SCM_UNDEFINED);
	  for_const_reverse_contents (vector<sstring>, state_tokens, tok)
	    state_tuple_scm = scm_cons (scm_from_locale_string (tok->c_str()), state_tuple_scm);

	  SCM knowledge_result_scm = scm_call_3 (knowledge_func_scm, current_name_scm, node_name_scm, state_tuple_scm);
	  if (!scm_boolean_p (knowledge_result_scm))
	    THROWEXPR ("(" TERMINATRIX_MODEL << " (" TERMINATRIX_KNOWLEDGE_SCM " func) ...)  means  (func " << current_name << " " << node_name << " (" << state_tokens << ")) should return #t or #f, but it didn't");
	  const bool knowledge_result_true = knowledge_result_scm == SCM_BOOL_T;
	  if (CTAGGING(3,TERMINATRIX))
	    CL << "(" TERMINATRIX_KNOWLEDGE_SCM " " << current_name << " " << node_name << " (" << state_tokens << "))  =  " << (knowledge_result_true ? "#t" : "#f") << '\n';
	  const Score knowledge_result_score = knowledge_result_true ? ScoreOfProb1 : ScoreOfProb0;
	  node_ssm.insert (Symbol_score (state, knowledge_result_score));
	}

      // copy into named_node_scores
      named_node_scores[node_name] = node_ssm;
    }

  vector<const Symbol_score_map*> node_scores;
  const Symbol_score_map& wild_ssm = terminatrix.rate_matrix().alphabet().wild_ssm;
  // fill node_scores by looking up named nodes in named_node_scores, and using wildcards for unnamed nodes
  for (Phylogeny::Node n = 0; n < current_tree->nodes(); ++n)
    if (current_tree->node_name[n].size())
      node_scores.push_back (&named_node_scores[current_tree->node_name[n]]);
    else
      node_scores.push_back (&wild_ssm);

  // initialise Column_matrix
  current_colmat.alloc (current_tree->nodes(), terminatrix.rate_matrix().number_of_states());
  current_colmat.initialise (*current_tree, node_scores);
}
