#ifndef __DATA_PROVIDER_HPP__
#define __DATA_PROVIDER_HPP__

#include <boost/shared_ptr.hpp>
#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <vector>
#include <exception>
#include "errors.hpp"
#include "btree/value.hpp"
#include "buffer_cache/transactor.hpp"

struct buffer_group_t {
    struct buffer_t {
        ssize_t size;
        void *data;
    };
    std::vector<buffer_t> buffers;
    void add_buffer(size_t s, void *d) {
        buffer_t b;
        b.size = s;
        b.data = d;
        buffers.push_back(b);
    }
    size_t get_size() const {
        size_t s = 0;
        for (int i = 0; i < (int)buffers.size(); i++) {
            s += buffers[i].size;
        }
        return s;
    }
};

struct const_buffer_group_t {
    struct buffer_t {
        ssize_t size;
        const void *data;
    };
    std::vector<buffer_t> buffers;
    void add_buffer(size_t s, const void *d) {
        buffer_t b;
        b.size = s;
        b.data = d;
        buffers.push_back(b);
    }
    size_t get_size() const {
        size_t s = 0;
        for (int i = 0; i < (int)buffers.size(); i++) {
            s += buffers[i].size;
        }
        return s;
    }
};

/* Data providers can throw data_provider_failed_exc_t to cancel the operation they are being used
for. In general no information can be carried along with the data_provider_failed_exc_t; it's meant
to signal to the data provider consumer, not the data provider creator. The cause of the error
should be communicated some other way. */

struct data_provider_failed_exc_t :
    public std::exception
{
    const char *what() {
        return "Data provider failed.";
    }
};

/* A data_provider_t conceptually represents a read-only array of bytes. It is an abstract
superclass; its concrete subclasses represent different sources of bytes.

In general, the data on a data_provider_t can only be requested once: once get_data_*() or discard()
has been called, they cannot be called again. This is to make it easier to implement data providers
that read off a socket or other one-time-use source of data. Note that it's not mandatory to read
the data at all--if a data provider really needs its data to be read, it must do it itself in the
destructor. */

struct data_provider_t {
    virtual ~data_provider_t() { }

    /* Consumers can call get_size() to figure out how many bytes long the byte array is. Producers
    should override get_size(). */
    virtual size_t get_size() const = 0;

    /* Consumers can call get_data_into_buffers() to ask the data_provider_t to fill a set of
    buffers that are provided. Producers should override get_data_into_buffers(). Alternatively,
    subclass from auto_copying_data_provider_t to get this behavior automatically in terms of
    get_data_as_buffers(). */
    virtual void get_data_into_buffers(const buffer_group_t *dest) throw (data_provider_failed_exc_t) = 0;

    /* Consumers can call get_data_as_buffers() to ask the data_provider_t to provide a set of
    buffers that already contain the data. The reason for this alternative interface is that some
    data providers already have the data in buffers, so this is more efficient than doing an extra
    copy. The buffers are guaranteed to remain valid until the data provider is destroyed. Producers
    should also override get_data_as_buffers(), or subclass from auto_buffering_data_provider_t to
    automatically implement it in terms of get_data_into_buffers(). */
    virtual const const_buffer_group_t *get_data_as_buffers() throw (data_provider_failed_exc_t) = 0;
};

/* A auto_buffering_data_provider_t is a subclass of data_provider_t that provides an implementation
of get_data_as_buffers() in terms of get_data_into_buffers(). It is itself an abstract class;
subclasses should override get_size() and get_data_into_buffers(). */

struct auto_buffering_data_provider_t : public data_provider_t {
    const const_buffer_group_t *get_data_as_buffers() throw (data_provider_failed_exc_t);
private:
    boost::scoped_array<char> buffer;   /* This is NULL until buffers are requested */
    const_buffer_group_t buffer_group;
};

/* A auto_copying_data_provider_t is a subclass of data_provider_t that implements
get_data_into_buffers() in terms of get_data_as_buffers(). It is itself an abstract class;
subclasses should override get_size(), get_data_as_buffers(), and done_with_buffers(). */

struct auto_copying_data_provider_t : public data_provider_t {
    void get_data_into_buffers(const buffer_group_t *dest) throw (data_provider_failed_exc_t);
};

/* A buffered_data_provider_t is a data_provider_t that simply owns an internal buffer that it
provides the data from. */

struct buffered_data_provider_t : public auto_copying_data_provider_t {
    buffered_data_provider_t(data_provider_t *dp);   // Create with contents of another
    buffered_data_provider_t(const void *, size_t);   // Create by copying out of a buffer
    buffered_data_provider_t(size_t, void **);    // Allocate buffer, let creator fill it
    size_t get_size() const;
    const const_buffer_group_t *get_data_as_buffers() throw (data_provider_failed_exc_t);
private:
    size_t size;
    const_buffer_group_t bg;
    boost::scoped_array<char> buffer;
};

/* maybe_buffered_data_provider_t wraps another data_provider_t. It acts exactly like the
data_provider_t it wraps, even down to throwing the same exceptions in the same places. Internally,
it buffers the other data_provider_t if it is sufficiently small, improving performance. */

struct maybe_buffered_data_provider_t : public data_provider_t {
    maybe_buffered_data_provider_t(data_provider_t *dp, int threshold);

    size_t get_size() const;
    void get_data_into_buffers(const buffer_group_t *dest) throw (data_provider_failed_exc_t);
    const const_buffer_group_t *get_data_as_buffers() throw (data_provider_failed_exc_t);

private:
    int size;
    data_provider_t *original;
    // true if we decide to buffer but there is an exception. We catch the exception in the
    // constructor and then set this variable to true, then throw data_provider_failed_exc_t()
    // when our data is requested. This way we behave exactly the same whether or not we buffer.
    bool exception_was_thrown;
    boost::scoped_ptr<buffered_data_provider_t> buffer;   // NULL if we decide not to buffer
};

class value_data_provider_t : public auto_copying_data_provider_t {
protected:
    value_data_provider_t() { }
public:
    virtual ~value_data_provider_t() { }

    static value_data_provider_t *create(const btree_value *value, const boost::shared_ptr<transactor_t>& transactor);
};

class small_value_data_provider_t : public value_data_provider_t {
private:
    small_value_data_provider_t(const btree_value *value);

public:
    size_t get_size() const;
    const const_buffer_group_t *get_data_as_buffers() throw (data_provider_failed_exc_t);

private:
    typedef std::vector<byte> buffer_t;
    buffer_t value;
    boost::scoped_ptr<const_buffer_group_t> buffers;

    friend class value_data_provider_t;
};

class large_value_data_provider_t : public value_data_provider_t {
private:
    large_value_data_provider_t(const btree_value *value, const boost::shared_ptr<transactor_t>& transactor);

public:
    virtual ~large_value_data_provider_t();

    size_t get_size() const;
    const const_buffer_group_t *get_data_as_buffers() throw (data_provider_failed_exc_t);

private:
    boost::shared_ptr<transactor_t> transactor;
    boost::scoped_ptr<const_buffer_group_t> buffers;
    boost::scoped_ptr<large_buf_t> large_value;
    large_buf_ref lb_ref;

    friend class value_data_provider_t;
};

#endif /* __DATA_PROVIDER_HPP__ */
