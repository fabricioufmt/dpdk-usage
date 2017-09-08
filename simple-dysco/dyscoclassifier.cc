#include <click/config.h>
#include "dyscoclassifier.hh"
#include <click/glue.hh>
#include <click/error.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <string>

#include <click/batchelement.hh>

CLICK_DECLS

Vector<DyscoHeader> dyscolist;

DyscoClassifier::DyscoClassifier()
{
}

DyscoClassifier::~DyscoClassifier()
{
}

int
DyscoClassifier::configure(Vector<String> &conf, ErrorHandler *errh)
{
	Vector<String> new_conf;
	for (int i = 0; i < conf.size(); i++)
		new_conf.push_back(String(i) + " " + conf[i]);
    	int r = IPFilter::configure(new_conf, errh);
    	if (r >= 0 && !router()->initialized())
		_zprog.warn_unused_outputs(noutputs(), errh);
    	return r;
}

// conf: <priority> <sc_len> <chain[ chain]> <filter>
int DyscoClassifier::add_policy(const String& conf, Element* e, void* thunk, ErrorHandler* errh) {
 	int i;
	int j;
	char* p;
	int offset;
	DyscoHeader elem;

	offset = 0;
	p = strtok(const_cast<char*>(conf.c_str()), " ");
	elem.priority = atoi(p);
	offset += strlen(p) + 1;
	p = strtok(NULL, " ");
	elem.sc_len = atoi(p);
	elem.chain_len = 0;
	offset += strlen(p) + 1;
	for(i = 0; i < elem.sc_len; i++) {
		p = strtok(NULL, " ");
		offset += strlen(p) + 1;
		elem.chain.push_back(p);
		elem.chain[i].append(" ");
		elem.chain_len += elem.chain[i].length();
	}

	elem.filter = String(const_cast<char*>(conf.c_str()) + offset);

	Vector<DyscoHeader>::iterator it; 
	bool added = false;
	for(i = 0, it = dyscolist.begin(); it < dyscolist.end(); i++, it++) {
		if(elem.priority < dyscolist[i].priority) {
			dyscolist.insert(it, elem);
			added = true;
			break;
    		}
  	}
  
	if(!added)
		dyscolist.push_back(elem);

	Vector<String> new_conf;
	for(j = 0; j < dyscolist.size(); j++)
		new_conf.push_back(String(j) + " " + dyscolist[j].filter);
	new_conf.push_back(String(j) + " -");		
  
	IPFilterProgram zprog;
	parse_program(zprog, new_conf, new_conf.size(), e, errh);

	if(!errh->nerrors()) {
		(static_cast<DyscoClassifier*>(e))->_zprog = zprog;
		return 0;
	}

	if(!added)
		dyscolist.pop_back();
	else
		dyscolist.erase(dyscolist.begin() + i);
	
	return 1;
}

//conf: <prio> <filter>
int DyscoClassifier::remove_policy(const String& conf, Element* e, void* thunk, ErrorHandler* errh) {
	char* p = strtok(const_cast<char*>(conf.c_str()), " ");
	int priority = atoi(p);

	String filter(const_cast<char*>(conf.c_str()) + strlen(p) + 1);

	int i;
	Vector<DyscoHeader>::iterator it;
	for(i = 0, it = dyscolist.begin(); it < dyscolist.end(); it++, i++) {
		DyscoHeader elem = dyscolist[i];
		if(elem.priority == priority && elem.filter == filter)
			dyscolist.erase(it);
	}

	int j;
	Vector<String> new_conf;
	for(j = 0; j < dyscolist.size(); j++)
    		new_conf.push_back(String(j) + " " + dyscolist[j].filter);
	
	new_conf.push_back(String(j) + " -");		
  
  	IPFilterProgram zprog;
  	parse_program(zprog, new_conf, new_conf.size(), e, errh);
  	if(!errh->nerrors()) {
    		(static_cast<DyscoClassifier*>(e))->_zprog = zprog;
    		return 0;
  	}

  	return 1;
}

String DyscoClassifier::list_policy(Element* e, void* thunk) {
	String buff;
	char buf[128];
	buff.append("prio\tsc_len\tchain\t\t\tfilter\n");
	for(int i = 0; i < dyscolist.size(); i++) {
		snprintf(buf, sizeof(buf), "%d\t", dyscolist[i].priority);
		buff.append(buf);
		snprintf(buf, sizeof(buf), "%d\t", dyscolist[i].sc_len);
		buff.append(buf);
		for(int j = 0; j < dyscolist[i].chain.size(); j++)
			buff.append(dyscolist[i].chain[j]);
		buff.append("\t\t");
		buff.append(dyscolist[i].filter);
		buff.append("\n");
	}
	buff.append("99\t-\t(null)\t\t\t-\n");
	return buff;
}

void DyscoClassifier::add_handlers() {
	IPFilter::add_handlers();
	add_write_handler("add", add_policy, 0);
	add_write_handler("remove", remove_policy, 0);
	add_read_handler("list", list_policy);
}

void DyscoClassifier::setpayload(DyscoHeader el, PacketBatch* batch) {
	int offset = batch->length();
	WritablePacket* q = batch->put(el.chain_len);

	for(int i = 0; i < el.chain.size(); i++) {
		memcpy(q->data() + offset, el.chain[i].c_str(), el.chain[i].length());
		offset += el.chain[i].length();
	}

	output(0).push(q);
}

void DyscoClassifier::dysco_checked_output_push_batch(int port, PacketBatch* batch) {
	if((unsigned) port <= (unsigned) dyscolist.size()) {
#if BATCH_DEBUG
		assert(in_batch_mode == BATCH_MODE_YES);
#endif
		if(port == dyscolist.size()) 
			output_push_batch(1, batch);
		else
			setpayload(dyscolist[port], batch);
	} else
		batch->fast_kill();
}

void DyscoClassifier::dysco_classify_each_packet(int nbatches, PacketBatch* batch) {
  PacketBatch* out[nbatches];
  bzero(out, sizeof(PacketBatch*) * nbatches);
  PacketBatch* cep_next = ((batch != NULL) ? static_cast<PacketBatch*>(batch->next()) : NULL );
  PacketBatch* p = batch;
  PacketBatch* last = NULL;
  int last_o = -1;
  int passed = 0;
  for(; p != NULL; p=cep_next, cep_next = (p == 0 ? 0 : static_cast<PacketBatch*>(p->next()))) {
    int o = match(p);
    if(o < 0 || o >= (nbatches))
      o = (nbatches - 1);
    if (o == last_o) {
      passed ++;
    } else {
      if (last == NULL) {
	out[o] = p;
	p->set_count(1);
	p->set_tail(p);
      } else {
	out[last_o]->set_tail(last);
	out[last_o]->set_count(out[last_o]->count() + passed);
	if (!out[o]) {
	  out[o] = p;
	  out[o]->set_count(1);
	  out[o]->set_tail(p);
	} else {
	  out[o]->append_packet(p);
	}
	passed = 0;
      }
    }
    last = p;
    last_o = o;
  }
  if(passed) {
    out[last_o]->set_tail(last);
    out[last_o]->set_count(out[last_o]->count() + passed);
  }
  int i = 0;
  for(; i < nbatches; i++) {
    if(out[i]) {
      out[i]->tail()->set_next(NULL);
      dysco_checked_output_push_batch(i, out[i]);
    }
  }
}

#if HAVE_BATCH
void DyscoClassifier::push_batch(int port, PacketBatch* batch) {
	//CLASSIFY_EACH_PACKET( (noutputs() + 1), match, batch, checked_output_push_batch);
	dysco_classify_each_packet(dyscolist.size() + 1, batch);
}
#endif

CLICK_ENDDECLS
ELEMENT_REQUIRES(IPFilter)
EXPORT_ELEMENT(DyscoClassifier)
ELEMENT_MT_SAFE(DyscoClassifier)
