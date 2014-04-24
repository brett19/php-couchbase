<?php
/**
 * The public facing API of the PHP Couchbase Client.
 *
 * @author Brett Lawson
 * @version 2.0.0
 * @package Couchbase
 */

/**
 * Represents a cluster connection.
 *
 * @package Couchbase
 */
class CouchbaseCluster {
    /**
     * @var _CouchbaseCluster
     * @ignore
     *
     * Pointer to our C binding backing class.
     */
    private $_me;

    /**
     * @var array
     * @ignore
     *
     * A list of hosts part of this cluster.
     */
    private $_hosts;

    /**
     * @var string
     * @ignore
     *
     * The path to the configuration cache file.
     */
    private $_ccpath;

    /**
     * Creates a connection to a cluster.
     *
     * Creates a CouchbaseCluster object and begins the bootstrapping
     * process neccessary for communications with the Couchbase Server.
     *
     * @param string|array $hosts A string or array of hosts of Couchbase nodes.
     * @param string $username The username for the cluster.
     * @param string $password The password for the cluster.
     * @param boolean $usessl Whether to use secure connections to the cluster.
     * @param string $ccpath Path to the configuration cache file.
     *
     * @throws CouchbaseException
     */
    public function __construct($hosts = array('127.0.0.1:8091'), $username = '', $password = '', $usessl = false, $ccpath = '') {
        $this->_me = new _CouchbaseCluster($hosts, $username, $password, $ccpath);
        $this->_hosts = $hosts;
        $this->_ccpath = $ccpath;
    }

    /**
     * Constructs a connection to a bucket.
     *
     * @param string $name The name of the bucket to open.
     * @param string $password The bucket password to authenticate with.
     * @return CouchbaseBucket A bucket object.
     *
     * @throws CouchbaseException
     *
     * @see CouchbaseBucket CouchbaseBucket
     */
    public function openBucket($name = 'default', $password = '') {
        return new CouchbaseBucket($this->_hosts, $name, $password, $this->_ccpath);
    }

    /**
     * Constructs a querier object for performing N1QL queries.
     *
     * This function will create a querier object which can be used for
     * performing N1QL queries against a cluster.
     *
     * @param string|array $queryhosts A string or array of hosts of cbq_engine instances.
     * @return CouchbaseQuerier A querier object.
     *
     * @throws CouchbaseException
     *
     * @see CouchbaseQuerier CouchbaseQuerier
     */
    public function openQuerier($queryhosts) {
        if (!is_array($queryhosts)) {
            $hosts = array();
            array_push($hosts, $queryhosts);
        } else {
            $hosts = $queryhosts;
        }

        return new CouchbaseQuerier($hosts);
    }

    /**
     * Connects to management services on the cluster.
     *
     * You must use this function if you wish to perform management
     * activities using this cluster instance.
     *
     * @throws CouchbaseException
     */
    public function connect() {
        return $this->_me->connect();
    }

    /**
     * Retrieves cluster status information
     *
     * Returns an associative array of status information as seen
     * on the cluster.  The exact structure of the returned data
     * can be seen in the Couchbase Manual by looking at the
     * cluster /info endpoint.
     *
     * @return mixed The status information.
     *
     * @throws CouchbaseException
     */
    public function info() {
        $path = "/pools/default";
        $res = $this->me->http_request(2, 1, $path, NULL, 2);
        return json_decode($res, true);
    }
}

/**
 * Represents a query connection.
 *
 * Note: This class must be constructed by calling the openQuerier
 * method of the CouchbaseCluster class.
 *
 * @package Couchbase
 *
 * @see CouchbaseCluster::openQuerier()
 */
class CouchbaseQuerier {
    /**
     * @var array
     * @ignore
     *
     * A host list for cbq_engine instances.
     */
    private $_hosts;

    /**
     * Constructs a querier object.
     *
     * @private
     * @ignore
     */
    public function __construct($hosts) {
        $this->_hosts = $hosts;
    }

    /**
     * Executes a N1QL query.
     *
     * @param $dmlstring The N1QL query to execute.
     * @return array The resultset of the query.
     *
     * @throws CouchbaseException
     */
    public function query($dmlstring) {
        $hostidx = array_rand($this->_hosts, 1);
        $host = $this->_hosts[$hostidx];

        $ch = curl_init();
        curl_setopt($ch, CURLOPT_URL, 'http://' . $host . '/query');
        curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);
        curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'POST');
        curl_setopt($ch, CURLOPT_POSTFIELDS, $dmlstring);
        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);
        curl_setopt($ch, CURLOPT_HTTPHEADER, array(
            'Content-Type: text/plain',
            'Content-Length: ' . strlen($dmlstring))
        );
        $result = curl_exec($ch);
        curl_close($ch);

        $resjson = json_decode($result, true);

        if (isset($resjson['error'])) {
            throw new CouchbaseException($resjson['error']['cause'], 999);
        }

        return $resjson['resultset'];
    }
}

/**
 * Represents a durability enforced bucket connection.
 *
 * Note: This class must be constructed by calling the endure
 * method of the CouchbaseBucket class.
 *
 * @property integer $operationTimeout
 * @property integer $viewTimeout
 * @property integer $durabilityInterval
 * @property integer $durabilityTimeout
 * @property integer $httpTimeout
 * @property integer $configTimeout
 * @property integer $configDelay
 * @property integer $configNodeTimeout
 * @property integer $htconfigIdleTimeout
 *
 * @private
 * @package Couchbase
 *
 * @see CouchbaseBucket::endure()
 */
class CouchbaseBucketDProxy {
    /**
     * @var CouchbaseBucket
     * @ignore
     *
     * A pointer back to the original CouchbaseBucket object.
     */
    private $_me;

    /**
     * @var string
     * @ignore
     *
     * The name of the bucket this object represents.
     */
    private $_bucket;

    /**
     * @var int
     * @ignore
     *
     * The level of persistence to enforce.
     */
    private $_persist;

    /**
     * @var int
     * @ignore
     *
     * The level of replication to enforce.
     */
    private $_replicate;

    /**
     * Constructs a bucket endure proxy.
     *
     * @private
     * @ignore
     *
     * @param $me Pointer to the creating bucket object.
     * @param $bucket Name of the bucket.
     * @param $persist The level of persistence requested.
     * @param $replicate The level of replication requested.
     */
    public function __construct($me, $bucket, $persist, $replicate) {
        $this->_me = $me;
        $this->_bucket = $bucket;
        $this->_persist = $persist;
        $this->_replicate = $replicate;

        // Implicit view durability
        if ($this->_persist < 1) {
            $this->_persist = 1;
        }
    }

    /**
     * Ensures durability requirements are met for an executed
     *  operation.  Note that this function will automatically
     *  determine the result types and check for any failures.
     *
     * @private
     * @ignore
     *
     * @param $id
     * @param $res
     * @return mixed
     * @throws Exception
     */
    private function _endure($id, $res) {
        if (is_array($res)) {
            // Build list of keys to check
            $chks = array();
            foreach ($res as $key => $result) {
                if (!$result->error) {
                    $chks[$key] = array(
                        'cas' => $result->cas
                    );
                }
            }

            // Do the checks
            $dres = $this->_me->durability($chks, array(
                'persist_to' => $this->_persist,
                'replicate_to' => $this->_replicate
            ));

            // Copy over the durability errors
            foreach ($dres as $key => $result) {
                if (!$result) {
                    $res[$key]->error = $result->error;
                }
            }

            return $res;
        } else {
            if ($res->error) {
                return $res;
            }

            $dres = $this->_me->durability(array(
                $id => array('cas' => $res->cas)
            ), array(
                'persist_to' => $this->_persist,
                'replicate_to' => $this->_replicate
            ));

            if ($dres) {
                return $res;
            } else {
                throw new Exception('durability requirements failed');
            }
        }
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::insert() CouchbaseBucket::insert()
     *
     * @param $id
     * @param $doc
     * @param array $options
     * @return mixed
     */
    public function insert($id, $doc, $options = array()) {
        return $this->_endure($id,
            $this->_bucket->insert($id, $doc, $options)
        );
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::upsert() CouchbaseBucket::upsert()
     *
     * @param $id
     * @param $doc
     * @param array $options
     * @return mixed
     */
    public function upsert($id, $doc, $options = array()) {
        return $this->_endure($id,
            $this->_bucket->upsert($id, $doc, $options)
        );
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::save() CouchbaseBucket::save()
     *
     * @param $id
     * @param $doc
     * @param array $options
     * @return mixed
     */
    public function save($id, $doc, $options = array()) {
        return $this->_endure($id,
            $this->_bucket->save($id, $doc, $options)
        );
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::update() CouchbaseBucket::update()
     *
     * @param $id
     * @param $updates
     * @param array $options
     * @return mixed
     */
    public function update($id, $updates, $options = array()) {
        return $this->_endure($id,
            $this->_bucket->update($id, $updates, $options)
        );
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::remove() CouchbaseBucket::remove()
     *
     * @param $id
     * @param array $options
     * @return mixed
     */
    public function remove($id, $options = array()) {
        return $this->_endure($id,
            $this->_bucket->remove($id, $options)
        );
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::get() CouchbaseBucket::get()
     *
     * @param $id
     * @param array $options
     * @return mixed
     */
    public function get($id, $options = array()) {
        return $this->_bucket->get($id, $options);
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::find() CouchbaseBucket::find()
     *
     * @param $ddocview
     * @param array $options
     * @return mixed
     */
    public function find($ddocview, $options = array()) {
        $options['stale'] = 'false';
        return $this->_bucket->find($ddocview, $options);
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::unlock() CouchbaseBucket::unlock()
     *
     * @param $id
     * @param $cas
     * @param array $options
     * @return mixed
     */
    public function unlock($id, $cas, $options = array()) {
        return $this->_bucket->unlock($id, $cas, $options);
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::counter() CouchbaseBucket::counter()
     *
     * @param $id
     * @param $delta
     * @param array $options
     * @return mixed
     */
    public function counter($id, $delta, $options = array()) {
        return $this->_endure($id,
            $this->_bucket->counter($id, $delta, $options)
        );
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::view() CouchbaseBucket::view()
     *
     * @param $ddoc
     * @param $view
     * @param array $options
     * @return mixed
     */
    public function view($ddoc, $view, $options = array()) {
        return $this->_bucket->view($ddoc, $view, $options);
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::info() CouchbaseBucket::info()
     *
     * @return mixed
     */
    public function info() {
        return $this->_bucket->info();
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::flush() CouchbaseBucket::flush()
     *
     * @return mixed
     */
    public function flush() {
        return $this->_bucket->flush();
    }

    /**
     * See referenced CouchbaseBucket parent method.
     *
     * @see CouchbaseBucket::endure() CouchbaseBucket::endure()
     *
     * @param array $reqs
     * @return CouchbaseBucketDProxy
     */
    public function endure($reqs = array()) {
        if (isset($reqs['persist_to'])) {
            $reqs['persist_to'] = max($reqs['persist_to'], $this->_persist);
        } else {
            $reqs['persist_to'] = $this->_persist;
        }
        if (isset($reqs['replicate_to'])) {
            $reqs['replicate_to'] = max($reqs['replicate_to'], $this->_persist);
        } else {
            $reqs['replicate_to'] = $this->_replicate;
        }
        return $this->_me->endure($reqs);
    }

    /**
     * Magic function to handle the retrieval of various properties.  This
     * simply proxies the request to the underlying bucket object.
     *
     * @internal
     */
    public function __get($name) {
        return $this->_me->__get($name);
    }

    /**
     * Magic function to handle the setting of various properties.  This
     * simply proxies the request to the underlying bucket object.
     *
     * @internal
     */
    public function __set($name, $value) {
        return $this->_me->__set($name, $value);
    }
}

/**
 * Represents a bucket connection.
 *
 * Note: This class must be constructed by calling the openBucket
 * method of the CouchbaseCluster class.
 *
 * @property integer $operationTimeout
 * @property integer $viewTimeout
 * @property integer $durabilityInterval
 * @property integer $durabilityTimeout
 * @property integer $httpTimeout
 * @property integer $configTimeout
 * @property integer $configDelay
 * @property integer $configNodeTimeout
 * @property integer $htconfigIdleTimeout
 *
 * @package Couchbase
 *
 * @see CouchbaseCluster::openBucket()
 */
class CouchbaseBucket {
    /**
     * @var _CouchbaseBucket
     * @ignore
     *
     * Pointer to our C binding backing class.
     */
    private $me;

    /**
     * @var string
     * @ignore
     *
     * The name of the bucket this object represents.
     */
    private $name;

    /**
     * Constructs a bucket connection.
     *
     * @private
     * @ignore
     *
     * @param $hosts A list of hosts for the cluster.
     * @param $name The name of the bucket to connect to.
     * @param $password The password to authenticate with.
     *
     * @private
     */
    public function __construct($hosts, $name, $password, $ccpath) {
        $this->me = new _CouchbaseBucket($hosts, $name, $password, $ccpath);
        $this->me->setTranscoder("couchbase_default_encoder", "couchbase_default_decoder");
        $this->name = $name;
    }

    /**
     * Inserts a document.  This operation will fail if
     * the document already exists on the cluster.
     *
     * @param string|array $ids
     * @param mixed $val
     * @param array $options
     * @return mixed
     */
    public function insert($ids, $val = NULL, $options = array()) {
        return $this->me->insert($ids, $val, $options);
    }

    /**
     * Inserts or updates a document, depending on whether the
     * document already exists on the cluster.
     *
     * @param string|array $ids
     * @param mixed $val
     * @param array $options
     * @return mixed
     */
    public function upsert($ids, $val = NULL, $options = array()) {
        return $this->me->upsert($ids, $val, $options);
    }

    /**
     * Saves a document.
     *
     * @param string|array $ids
     * @param mixed $val
     * @param array $options
     * @return mixed
     */
    public function save($ids, $val = NULL, $options = array()) {
        return $this->me->save($ids, $val, $options);
    }

    /**
     * Deletes a document.
     *
     * @param string|array $ids
     * @param array $options
     * @return mixed
     */
    public function remove($ids, $options = array()) {
        return $this->me->remove($ids, $options);
    }

    /**
     * Retrieves a document.
     *
     * @param string|array $ids
     * @param array $options
     * @return mixed
     */
    public function get($ids, $options = array()) {
        return $this->me->get($ids, $options);
    }

    /**
     * Increment or decrements a key (based on $delta).
     *
     * @param string|array $ids
     * @param integer $delta
     * @param array $options
     * @return mixed
     */
    public function counter($ids, $delta, $options = array()) {
        return $this->me->counter($ids, $delta, $options);
    }

    /**
     * Unlocks a key previous locked with a call to get().
     * @param string|array $ids
     * @param array $options
     * @return mixed
     */
    public function unlock($ids, $options = array()) {
        return $this->me->unlock($ids, $options);
    }

    /**
     * Finds a document by view request.
     *
     * @param string $ddocview
     * @param array $options
     * @return array
     */
    public function find($ddocview, $options) {
        $info = explode('/', $ddocview);
        $res = $this->view($info[0], $info[1], $options);
        $out = array();

        return $out;
    }

    /**
     * Performs a raw view request.
     *
     * @param string $ddoc
     * @param string $view
     * @param array $options
     * @return mixed
     * @throws CouchbaseException
     */
    public function view($ddoc, $view, $options) {
        $path = '/_design/' . $ddoc . '/_view/' . $view;
        $args = array();
        foreach ($options as $option => $value) {
            array_push($args, $option . '=' . json_encode($value));
        }
        $path .= '?' . implode('&', $args);
        $res = $this->me->http_request(1, 1, $path, NULL, 1);
        $out = json_decode($res, true);
        if ($out['error']) {
            throw new CouchbaseException($out['error'] . ': ' . $out['reason']);
        }
        return $out;
    }

    /**
     * Flushes a bucket (clears all data).
     *
     * @return mixed
     */
    public function flush() {
        return $this->me->flush();
    }

    /**
     * Retrieves bucket status information
     *
     * Returns an associative array of status information as seen
     * by the cluster for this bucket.  The exact structure of the
     * returned data can be seen in the Couchbase Manual by looking
     * at the bucket /info endpoint.
     *
     * @return mixed The status information.
     */
    public function info() {
        $path = "/pools/default/buckets/" . $this->name;
        $res = $this->me->http_request(2, 1, $path, NULL, 2);
        return json_decode($res, true);
    }

    /**
     * Creates a proxy bucket object that will ensure a certain
     * level of durability is achieved.
     *
     * @param array $reqs
     * @return CouchbaseBucketDProxy
     */
    public function endure($reqs = array()) {
        $persist = 0;
        if (isset($reqs['persist_to'])) {
            $persist = $reqs['persist_to'];
        }
        $replicate = 0;
        if (isset($reqs['replicate_to'])) {
            $replicate = $reqs['replicate_to'];
        }
        return new CouchbaseBucketDProxy($this->me, $this, $persist, $replicate);
    }

    /**
     * Magic function to handle the retrieval of various properties.
     *
     * @internal
     */
    public function __get($name) {
        if ($name == 'operationTimeout') {
            return $this->me->getOption(COUCHBASE_CNTL_OP_TIMEOUT);
        } else if ($name == 'viewTimeout') {
            return $this->me->getOption(COUCHBASE_CNTL_VIEW_TIMEOUT);
        } else if ($name == 'durabilityInterval') {
            return $this->me->getOption(COUCHBASE_CNTL_DURABILITY_INTERVAL);
        } else if ($name == 'durabilityTimeout') {
            return $this->me->getOption(COUCHBASE_CNTL_DURABILITY_TIMEOUT);
        } else if ($name == 'httpTimeout') {
            return $this->me->getOption(COUCHBASE_CNTL_HTTP_TIMEOUT);
        } else if ($name == 'configTimeout') {
            return $this->me->getOption(COUCHBASE_CNTL_CONFIGURATION_TIMEOUT);
        } else if ($name == 'configDelay') {
            return $this->me->getOption(COUCHBASE_CNTL_CONFDELAY_THRESH);
        } else if ($name == 'configNodeTimeout') {
            return $this->me->getOption(COUCHBASE_CNTL_CONFIG_NODE_TIMEOUT);
        } else if ($name == 'htconfigIdleTimeout') {
            return $this->me->getOption(COUCHBASE_CNTL_HTCONFIG_IDLE_TIMEOUT);
        }


        $trace = debug_backtrace();
        trigger_error(
            'Undefined property via __get(): ' . $name .
            ' in ' . $trace[0]['file'] .
            ' on line ' . $trace[0]['line'],
            E_USER_NOTICE);
        return null;
    }

    /**
     * Magic function to handle the setting of various properties.
     *
     * @internal
     */
    public function __set($name, $value) {
        if ($name == 'operationTimeout') {
            return $this->me->setOption(COUCHBASE_CNTL_OP_TIMEOUT, $value);
        } else if ($name == 'viewTimeout') {
            return $this->me->setOption(COUCHBASE_CNTL_VIEW_TIMEOUT, $value);
        } else if ($name == 'durabilityInterval') {
            return $this->me->setOption(COUCHBASE_CNTL_DURABILITY_INTERVAL, $value);
        } else if ($name == 'durabilityTimeout') {
            return $this->me->setOption(COUCHBASE_CNTL_DURABILITY_TIMEOUT, $value);
        } else if ($name == 'httpTimeout') {
            return $this->me->setOption(COUCHBASE_CNTL_HTTP_TIMEOUT, $value);
        } else if ($name == 'configTimeout') {
            return $this->me->setOption(COUCHBASE_CNTL_CONFIGURATION_TIMEOUT, $value);
        } else if ($name == 'configDelay') {
            return $this->me->setOption(COUCHBASE_CNTL_CONFDELAY_THRESH, $value);
        } else if ($name == 'configNodeTimeout') {
            return $this->me->setOption(COUCHBASE_CNTL_CONFIG_NODE_TIMEOUT, $value);
        } else if ($name == 'htconfigIdleTimeout') {
            return $this->me->setOption(COUCHBASE_CNTL_HTCONFIG_IDLE_TIMEOUT, $value);
        }


        $trace = debug_backtrace();
        trigger_error(
            'Undefined property via __set(): ' . $name .
            ' in ' . $trace[0]['file'] .
            ' on line ' . $trace[0]['line'],
            E_USER_NOTICE);
        return null;
    }
}

/*
 * The following is a list of constants used for flags and datatype
 * encoding and decoding by the built in transcoders.
 */

/** @internal */ define('COUCHBASE_VAL_MASK', 0x1F);
/** @internal */ define('COUCHBASE_VAL_IS_STRING', 0);
/** @internal */ define('COUCHBASE_VAL_IS_LONG', 1);
/** @internal */ define('COUCHBASE_VAL_IS_DOUBLE', 2);
/** @internal */ define('COUCHBASE_VAL_IS_BOOL', 3);
/** @internal */ define('COUCHBASE_VAL_IS_SERIALIZED', 4);
/** @internal */ define('COUCHBASE_VAL_IS_IGBINARY', 5);
/** @internal */ define('COUCHBASE_VAL_IS_JSON', 6);
/** @internal */ define('COUCHBASE_COMPRESSION_MASK', 0x7 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_NONE', 0 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_ZLIB', 1 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_FASTLZ', 2 << 5);
/** @internal */ define('COUCHBASE_COMPRESSION_MCISCOMPRESSED', 1 << 4);
/** @internal */ define('COUCHBASE_SERTYPE_JSON', 0);
/** @internal */ define('COUCHBASE_SERTYPE_IGBINARY', 1);
/** @internal */ define('COUCHBASE_SERTYPE_PHP', 2);
/** @internal */ define('COUCHBASE_CMPRTYPE_NONE', 0);
/** @internal */ define('COUCHBASE_CMPRTYPE_ZLIB', 1);
/** @internal */ define('COUCHBASE_CMPRTYPE_FASTLZ', 2);

/**
 * The default options for V1 encoding when using the default
 * transcoding functionality.
 * @internal
 */
$COUCHBASE_DEFAULT_ENCOPTS = array(
    'sertype' => COUCHBASE_SERTYPE_PHP,
    'cmprtype' => COUCHBASE_CMPRTYPE_NONE,
    'cmprthresh' => 2000,
    'cmprfactor' => 1.3
);

/**
 * Performs encoding of user provided types into binary form for
 * on the server according to the original PHP SDK specification.
 *
 * @internal
 *
 * @param $value The value passed by the user
 * @param $options Various encoding options
 * @return array An array specifying the bytes, flags and datatype to store
 */
function couchbase_basic_encoder_v1($value, $options) {
    $data = NULL;
    $flags = 0;
    $datatype = 0;

    $sertype = $options['sertype'];
    $cmprtype = $options['cmprtype'];
    $cmprthresh = $options['cmprthresh'];
    $cmprfactor = $options['cmprfactor'];

    $vtype = gettype($value);
    if ($vtype == 'string') {
        $flags = COUCHBASE_VAL_IS_STRING;
        $data = $value;
    } else if ($vtype == 'integer') {
        $flags = COUCHBASE_VAL_IS_LONG;
        $data = (string)$value;
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    } else if ($vtype == 'double') {
        $flags = COUCHBASE_VAL_IS_DOUBLE;
        $data = (string)$value;
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    } else if ($vtype == 'boolean') {
        $flags = COUCHBASE_VAL_IS_BOOL;
        $data = (string)$value;
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    } else {
        if ($sertype == COUCHBASE_SERTYPE_JSON) {
            $flags = COUCHBASE_VAL_IS_JSON;
            $data = json_encode($value);
        } else if ($sertype == COUCHBASE_SERTYPE_IGBINARY) {
            $flags = COUCHBASE_VAL_IS_IGBINARY;
            $data = igbinary_serialize($value);
        } else if ($sertype == COUCHBASE_SERTYPE_PHP) {
            $flags = COUCHBASE_VAL_IS_SERIALIZED;
            $data = serialize($value);
        }
    }

    if (strlen($data) < $cmprthresh) {
        $cmprtype = COUCHBASE_CMPRTYPE_NONE;
    }

    if ($cmprtype != COUCHBASE_CMPRTYPE_NONE) {
        $cmprdata = NULL;
        $cmprflags = COUCHBASE_COMPRESSION_NONE;

        if ($cmprtype == COUCHBASE_CMPRTYPE_ZLIB) {
            $cmprdata = gzencode($data);
            $cmprflags = COUCHBASE_COMPRESSION_ZLIB;
        } else if ($cmprtype == COUCHBASE_CMPRTYPE_FASTLZ) {
            $cmprdata = fastlz_compress($data);
            $cmprflags = COUCHBASE_COMPRESSION_FASTLZ;
        }

        if ($cmprdata != NULL) {
            if (strlen($data) > strlen($cmprdata) * $cmprfactor) {
                $data = $cmprdata;
                $flags |= $cmprflags;
                $flags |= COUCHBASE_COMPRESSION_MCISCOMPRESSED;
            }
        }
    }

    return array($data, $flags, $datatype);
}

/**
 * Performs decoding of the server provided binary data into
 * PHP types according to the original PHP SDK specification.
 *
 * @internal
 *
 * @param $bytes The binary received from the server
 * @param $flags The flags received from the server
 * @param $datatype The datatype received from the server
 * @return mixed The resulting decoded value
 */
function couchbase_basic_decoder_v1($bytes, $flags, $datatype) {
    $sertype = $flags & COUCHBASE_VAL_MASK;
    $cmprtype = $flags & COUCHBASE_COMPRESSION_MASK;

    $data = $bytes;
    if ($cmprtype == COUCHBASE_COMPRESSION_ZLIB) {
        $bytes = gzdecode($bytes);
    } else if ($cmprtype == COUCHBASE_COMPRESSION_FASTLZ) {
        $data = fastlz_decompress($bytes);
    }

    $retval = NULL;
    if ($sertype == COUCHBASE_VAL_IS_STRING) {
        $retval = $data;
    } else if ($sertype == COUCHBASE_VAL_IS_LONG) {
        $retval = intval($data);
    } else if ($sertype == COUCHBASE_VAL_IS_DOUBLE) {
        $retval = floatval($data);
    } else if ($sertype == COUCHBASE_VAL_IS_BOOL) {
        $retval = boolval($data);
    } else if ($sertype == COUCHBASE_VAL_IS_JSON) {
        $retval = json_decode($data);
    } else if ($sertype == COUCHBASE_VAL_IS_IGBINARY) {
        $retval = igbinary_unserialize($data);
    } else if ($sertype == COUCHBASE_VAL_IS_SERIALIZED) {
        $retval = unserialize($data);
    }

    return $retval;
}

/**
 * Default passthru encoder which simply passes data
 * as-is rather than performing any transcoding.
 *
 * @internal
 */
function couchbase_passthru_encoder($value) {
    return $value;
}

/**
 * Default passthru encoder which simply passes data
 * as-is rather than performing any transcoding.
 *
 * @internal
 */
function couchbase_passthru_decoder($bytes, $flags, $datatype) {
    return $bytes;
}

/**
 * The default encoder for the client.  Currently invokes the
 * v1 encoder directly with the default set of encoding options.
 *
 * @internal
 */
function couchbase_default_encoder($value) {
    global $COUCHBASE_DEFAULT_ENCOPTS;
    return couchbase_basic_encoder_v1($value, $COUCHBASE_DEFAULT_ENCOPTS);
}

/**
 * The default decoder for the client.  Currently invokes the
 * v1 decoder directly.
 *
 * @internal
 */
function couchbase_default_decoder($bytes, $flags, $datatype) {
    return couchbase_basic_decoder_v1($bytes, $flags, $datatype);
}
