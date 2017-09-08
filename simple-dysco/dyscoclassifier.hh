#ifndef CLICK_DYSCOCLASSIFIER_HH
#define CLICK_DYSCOCLASSIFIER_HH
#include "elements/ip/ipclassifier.hh"
CLICK_DECLS

typedef struct DyscoHeader {
	int priority;
	int sc_len;
	int chain_len;
	Vector<String> chain;
	String filter;
} DyscoHeader;

class DyscoClassifier : public IPClassifier { public:

  DyscoClassifier() CLICK_COLD;
  ~DyscoClassifier() CLICK_COLD;

  const char *class_name() const		{ return "DyscoClassifier"; }
  const char *processing() const		{ return PUSH; }

  int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;
  void add_handlers() CLICK_COLD;

#if HAVE_BATCH
  void push_batch(int, PacketBatch*);
#endif
   static int add_policy(const String&, Element*, void*, ErrorHandler*);
   static int remove_policy(const String&, Element*, void*, ErrorHandler*);
   static String list_policy(Element*, void*);
   void dysco_classify_each_packet(int, PacketBatch*);
   void setpayload(DyscoHeader, PacketBatch*);
   void dysco_checked_output_push_batch(int, PacketBatch*);
};

CLICK_ENDDECLS
#endif
