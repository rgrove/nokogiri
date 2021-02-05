#include <xml_namespace.h>

/*
 *  The lifecycle of a Namespace node is more complicated than other Nodes, for two reasons:
 *
 *  1. the underlying C structure has a different layout than all the other node structs, with the
 *     `_private` member where we store a pointer to Ruby object data not being in first position.
 *  2. xmlNs structures returned in an xmlNodeset from an XPath query are copies of the document's
 *     namespaces, and so do not share the same memory lifecycle as everything else in a document.
 *
 *  As a result of 1, you may see special handling of XML_NAMESPACE_DECL node types throughout the
 *  Nokogiri C code, though I intend to wrap up that logic in ruby_object_{get,set} functions
 *  shortly.
 *
 *  As a result of 2, you will see we have special handling in this file and in xml_node_set.c to
 *  carefully manage the memory lifecycle of xmlNs structs to match the Ruby object's GC
 *  lifecycle. In xml_node_set.c we have local versions of xmlXPathNodeSetDel() and
 *  xmlXPathFreeNodeSet() that avoid freeing xmlNs structs in the node set. In this file, we decide
 *  whether or not to call dealloc_namespace() depending on whether the xmlNs struct appears to be
 *  in an xmlNodeSet (and thus the result of an XPath query) or not.
 *
 *  Yes, this is madness.
 */

VALUE cNokogiriXmlNamespace ;

static void dealloc_namespace(xmlNsPtr ns)
{
  /*
   *
   * this deallocator is only used for namespace nodes that are part of an xpath
   * node set.
   *
   * see Nokogiri_wrap_xml_namespace() for more details.
   *
   */
  NOKOGIRI_DEBUG_START(ns) ;
  if (ns->href) {
    xmlFree((xmlChar *)(uintptr_t)ns->href);
  }
  if (ns->prefix) {
    xmlFree((xmlChar *)(uintptr_t)ns->prefix);
  }
  xmlFree(ns);
  NOKOGIRI_DEBUG_END(ns) ;
}


/*
 * call-seq:
 *  prefix
 *
 * Get the prefix for this namespace.  Returns +nil+ if there is no prefix.
 */
static VALUE prefix(VALUE self)
{
  xmlNsPtr ns;

  Data_Get_Struct(self, xmlNs, ns);
  if(!ns->prefix) return Qnil;

  return NOKOGIRI_STR_NEW2(ns->prefix);
}

/*
 * call-seq:
 *  href
 *
 * Get the href for this namespace
 */
static VALUE href(VALUE self)
{
  xmlNsPtr ns;

  Data_Get_Struct(self, xmlNs, ns);
  if(!ns->href) return Qnil;

  return NOKOGIRI_STR_NEW2(ns->href);
}

static int part_of_an_xpath_node_set_eh(xmlNsPtr node)
{
  return (node->next && ! NOKOGIRI_NAMESPACE_EH(node->next));
}

VALUE Nokogiri_wrap_xml_namespace(xmlDocPtr doc, xmlNsPtr node)
{
  VALUE ns = 0, document, node_cache;

  assert(doc->type == XML_DOCUMENT_NODE || doc->type == XML_HTML_DOCUMENT_NODE);

  if (node->_private) return (VALUE)node->_private;

  if (doc->type == XML_DOCUMENT_FRAG_NODE) doc = doc->doc;

  if (DOC_RUBY_OBJECT_TEST(doc)) {
    document = DOC_RUBY_OBJECT(doc);

    if (part_of_an_xpath_node_set_eh(node)) {
      /*
       *  this is a duplicate returned as part of an xpath query node set, and so
       *  we need to make sure we manage this memory.
       *
       *  see comments in xml_node_set.c for more details.
       */
      ns = Data_Wrap_Struct(cNokogiriXmlNamespace, 0, dealloc_namespace, node);
    } else {
      ns = Data_Wrap_Struct(cNokogiriXmlNamespace, 0, 0, node);
      node_cache = rb_iv_get(document, "@node_cache");
      rb_ary_push(node_cache, ns);
    }

    rb_iv_set(ns, "@document", document);
  } else {
    ns = Data_Wrap_Struct(cNokogiriXmlNamespace, 0, 0, node);
  }

  node->_private = (void *)ns;

  return ns;
}

void init_xml_namespace()
{
  VALUE nokogiri  = rb_define_module("Nokogiri");
  VALUE xml       = rb_define_module_under(nokogiri, "XML");
  VALUE klass     = rb_define_class_under(xml, "Namespace", rb_cObject);

  cNokogiriXmlNamespace = klass;

  rb_define_method(klass, "prefix", prefix, 0);
  rb_define_method(klass, "href", href, 0);
}
