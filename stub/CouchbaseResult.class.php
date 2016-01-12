<?php
/**
 * File for the CouchbaseResult class.
 *
 * @author Sylvain Robez-Masson <srm@bouh.org>
 */

/**
 * Represents a couchbase result.
 *
 * Note: This class must be constructed by calling the query
 * method of the CouchbaseBucket class.
 *
 * @property array $rows
 * @property integer $offset
 *
 * @package Couchbase
 *
 * @see CouchbaseBucket::query()
 */
class CouchbaseResult implements Iterator {

    /**
     * @var array
     * @ignore
     *
     * All the rows of the result.
     */
    private $rows;

    /**
     * @var integer
     * @ignore
     *
     * Offset of the iterator.
     */
    private $offset;

    /**
     * Constructs a couchbase result.
     *
     * @private
     * @ignore
     *
     * @param array $rows All the rows returned by a query.
     *
     * @private
     */
    public function __construct(array $rows) {
        $this->rows = $rows;
        $this->offset = 0;
    }

    /**
     * Retrieve current item of iterator.
     *
     * @return mixed
     */
    public function current()
    {
        return $this->rows[$this->offset];
    }

    /**
     * Iterator to the next row.
     *
     * @return void
     */
    public function next()
    {
        ++$this->offset;
    }

    /**
     * Retrieve key of the current row.
     *
     * @return int
     */
    public function key()
    {
        return $this->offset;
    }

    /**
     * Check if the iterator is still valid.
     *
     * @return bool
     */
    public function valid()
    {
        return isset($this->rows[$this->offset]);
    }

    /**
     * Rewind iterator
     *
     * @return void
     */
    public function rewind()
    {
        $this->offset = 0;
    }
}
