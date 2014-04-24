char *PCBC_PHP_CODESTR = \
"/**\n" \
" * The public facing API of the PHP Couchbase Client.\n" \
" *\n" \
" * @author Brett Lawson\n" \
" * @version 2.0.0\n" \
" * @package Couchbase\n" \
" */\n" \
"\n" \
"/**\n" \
" * Represents a cluster connection.\n" \
" *\n" \
" * @package Couchbase\n" \
" */\n" \
"class CouchbaseCluster {\n" \
"    /**\n" \
"     * @var _CouchbaseCluster\n" \
"     * @ignore\n" \
"     *\n" \
"     * Pointer to our C binding backing class.\n" \
"     */\n" \
"    private $_me;\n" \
"\n" \
"    /**\n" \
"     * @var array\n" \
"     * @ignore\n" \
"     *\n" \
"     * A list of hosts part of this cluster.\n" \
"     */\n" \
"    private $_hosts;\n" \
"\n" \
"    /**\n" \
"     * @var string\n" \
"     * @ignore\n" \
"     *\n" \
"     * The path to the configuration cache file.\n" \
"     */\n" \
"    private $_ccpath;\n" \
"\n" \
"    /**\n" \
"     * Creates a connection to a cluster.\n" \
"     *\n" \
"     * Creates a CouchbaseCluster object and begins the bootstrapping\n" \
"     * process neccessary for communications with the Couchbase Server.\n" \
"     *\n" \
"     * @param string|array $hosts A string or array of hosts of Couchbase nodes.\n" \
"     * @param string $username The username for the cluster.\n" \
"     * @param string $password The password for the cluster.\n" \
"     * @param boolean $usessl Whether to use secure connections to the cluster.\n" \
"     * @param string $ccpath Path to the configuration cache file.\n" \
"     *\n" \
"     * @throws CouchbaseException\n" \
"     */\n" \
"    public function __construct($hosts = array('127.0.0.1:8091'), $username = '', $password = '', $usessl = false, $ccpath = '') {\n" \
"        $this->_me = new _CouchbaseCluster($hosts, $username, $password, $ccpath);\n" \
"        $this->_hosts = $hosts;\n" \
"        $this->_ccpath = $ccpath;\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Constructs a connection to a bucket.\n" \
"     *\n" \
"     * @param string $name The name of the bucket to open.\n" \
"     * @param string $password The bucket password to authenticate with.\n" \
"     * @return CouchbaseBucket A bucket object.\n" \
"     *\n" \
"     * @throws CouchbaseException\n" \
"     *\n" \
"     * @see CouchbaseBucket CouchbaseBucket\n" \
"     */\n" \
"    public function openBucket($name = 'default', $password = '') {\n" \
"        return new CouchbaseBucket($this->_hosts, $name, $password, $this->_ccpath);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Constructs a querier object for performing N1QL queries.\n" \
"     *\n" \
"     * This function will create a querier object which can be used for\n" \
"     * performing N1QL queries against a cluster.\n" \
"     *\n" \
"     * @param string|array $queryhosts A string or array of hosts of cbq_engine instances.\n" \
"     * @return CouchbaseQuerier A querier object.\n" \
"     *\n" \
"     * @throws CouchbaseException\n" \
"     *\n" \
"     * @see CouchbaseQuerier CouchbaseQuerier\n" \
"     */\n" \
"    public function openQuerier($queryhosts) {\n" \
"        if (!is_array($queryhosts)) {\n" \
"            $hosts = array();\n" \
"            array_push($hosts, $queryhosts);\n" \
"        } else {\n" \
"            $hosts = $queryhosts;\n" \
"        }\n" \
"\n" \
"        return new CouchbaseQuerier($hosts);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Connects to management services on the cluster.\n" \
"     *\n" \
"     * You must use this function if you wish to perform management\n" \
"     * activities using this cluster instance.\n" \
"     *\n" \
"     * @throws CouchbaseException\n" \
"     */\n" \
"    public function connect() {\n" \
"        return $this->_me->connect();\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Retrieves cluster status information\n" \
"     *\n" \
"     * Returns an associative array of status information as seen\n" \
"     * on the cluster.  The exact structure of the returned data\n" \
"     * can be seen in the Couchbase Manual by looking at the\n" \
"     * cluster /info endpoint.\n" \
"     *\n" \
"     * @return mixed The status information.\n" \
"     *\n" \
"     * @throws CouchbaseException\n" \
"     */\n" \
"    public function info() {\n" \
"        $path = \"/pools/default\";\n" \
"        $res = $this->me->http_request(2, 1, $path, NULL, 2);\n" \
"        return json_decode($res, true);\n" \
"    }\n" \
"}\n" \
"\n" \
"/**\n" \
" * Represents a query connection.\n" \
" *\n" \
" * Note: This class must be constructed by calling the openQuerier\n" \
" * method of the CouchbaseCluster class.\n" \
" *\n" \
" * @package Couchbase\n" \
" *\n" \
" * @see CouchbaseCluster::openQuerier()\n" \
" */\n" \
"class CouchbaseQuerier {\n" \
"    /**\n" \
"     * @var array\n" \
"     * @ignore\n" \
"     *\n" \
"     * A host list for cbq_engine instances.\n" \
"     */\n" \
"    private $_hosts;\n" \
"\n" \
"    /**\n" \
"     * Constructs a querier object.\n" \
"     *\n" \
"     * @private\n" \
"     * @ignore\n" \
"     */\n" \
"    public function __construct($hosts) {\n" \
"        $this->_hosts = $hosts;\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Executes a N1QL query.\n" \
"     *\n" \
"     * @param $dmlstring The N1QL query to execute.\n" \
"     * @return array The resultset of the query.\n" \
"     *\n" \
"     * @throws CouchbaseException\n" \
"     */\n" \
"    public function query($dmlstring) {\n" \
"        $hostidx = array_rand($this->_hosts, 1);\n" \
"        $host = $this->_hosts[$hostidx];\n" \
"\n" \
"        $ch = curl_init();\n" \
"        curl_setopt($ch, CURLOPT_URL, 'http://' . $host . '/query');\n" \
"        curl_setopt($ch, CURLOPT_FOLLOWLOCATION, true);\n" \
"        curl_setopt($ch, CURLOPT_CUSTOMREQUEST, 'POST');\n" \
"        curl_setopt($ch, CURLOPT_POSTFIELDS, $dmlstring);\n" \
"        curl_setopt($ch, CURLOPT_RETURNTRANSFER, true);\n" \
"        curl_setopt($ch, CURLOPT_HTTPHEADER, array(\n" \
"            'Content-Type: text/plain',\n" \
"            'Content-Length: ' . strlen($dmlstring))\n" \
"        );\n" \
"        $result = curl_exec($ch);\n" \
"        curl_close($ch);\n" \
"\n" \
"        $resjson = json_decode($result, true);\n" \
"\n" \
"        if (isset($resjson['error'])) {\n" \
"            throw new CouchbaseException($resjson['error']['cause'], 999);\n" \
"        }\n" \
"\n" \
"        return $resjson['resultset'];\n" \
"    }\n" \
"}\n" \
"\n" \
"/**\n" \
" * Represents a durability enforced bucket connection.\n" \
" *\n" \
" * Note: This class must be constructed by calling the endure\n" \
" * method of the CouchbaseBucket class.\n" \
" *\n" \
" * @property integer $operationTimeout\n" \
" * @property integer $viewTimeout\n" \
" * @property integer $durabilityInterval\n" \
" * @property integer $durabilityTimeout\n" \
" * @property integer $httpTimeout\n" \
" * @property integer $configTimeout\n" \
" * @property integer $configDelay\n" \
" * @property integer $configNodeTimeout\n" \
" * @property integer $htconfigIdleTimeout\n" \
" *\n" \
" * @private\n" \
" * @package Couchbase\n" \
" *\n" \
" * @see CouchbaseBucket::endure()\n" \
" */\n" \
"class CouchbaseBucketDProxy {\n" \
"    /**\n" \
"     * @var CouchbaseBucket\n" \
"     * @ignore\n" \
"     *\n" \
"     * A pointer back to the original CouchbaseBucket object.\n" \
"     */\n" \
"    private $_me;\n" \
"\n" \
"    /**\n" \
"     * @var string\n" \
"     * @ignore\n" \
"     *\n" \
"     * The name of the bucket this object represents.\n" \
"     */\n" \
"    private $_bucket;\n" \
"\n" \
"    /**\n" \
"     * @var int\n" \
"     * @ignore\n" \
"     *\n" \
"     * The level of persistence to enforce.\n" \
"     */\n" \
"    private $_persist;\n" \
"\n" \
"    /**\n" \
"     * @var int\n" \
"     * @ignore\n" \
"     *\n" \
"     * The level of replication to enforce.\n" \
"     */\n" \
"    private $_replicate;\n" \
"\n" \
"    /**\n" \
"     * Constructs a bucket endure proxy.\n" \
"     *\n" \
"     * @private\n" \
"     * @ignore\n" \
"     *\n" \
"     * @param $me Pointer to the creating bucket object.\n" \
"     * @param $bucket Name of the bucket.\n" \
"     * @param $persist The level of persistence requested.\n" \
"     * @param $replicate The level of replication requested.\n" \
"     */\n" \
"    public function __construct($me, $bucket, $persist, $replicate) {\n" \
"        $this->_me = $me;\n" \
"        $this->_bucket = $bucket;\n" \
"        $this->_persist = $persist;\n" \
"        $this->_replicate = $replicate;\n" \
"\n" \
"        // Implicit view durability\n" \
"        if ($this->_persist < 1) {\n" \
"            $this->_persist = 1;\n" \
"        }\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Ensures durability requirements are met for an executed\n" \
"     *  operation.  Note that this function will automatically\n" \
"     *  determine the result types and check for any failures.\n" \
"     *\n" \
"     * @private\n" \
"     * @ignore\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param $res\n" \
"     * @return mixed\n" \
"     * @throws Exception\n" \
"     */\n" \
"    private function _endure($id, $res) {\n" \
"        if (is_array($res)) {\n" \
"            // Build list of keys to check\n" \
"            $chks = array();\n" \
"            foreach ($res as $key => $result) {\n" \
"                if (!$result->error) {\n" \
"                    $chks[$key] = array(\n" \
"                        'cas' => $result->cas\n" \
"                    );\n" \
"                }\n" \
"            }\n" \
"\n" \
"            // Do the checks\n" \
"            $dres = $this->_me->durability($chks, array(\n" \
"                'persist_to' => $this->_persist,\n" \
"                'replicate_to' => $this->_replicate\n" \
"            ));\n" \
"\n" \
"            // Copy over the durability errors\n" \
"            foreach ($dres as $key => $result) {\n" \
"                if (!$result) {\n" \
"                    $res[$key]->error = $result->error;\n" \
"                }\n" \
"            }\n" \
"\n" \
"            return $res;\n" \
"        } else {\n" \
"            if ($res->error) {\n" \
"                return $res;\n" \
"            }\n" \
"\n" \
"            $dres = $this->_me->durability(array(\n" \
"                $id => array('cas' => $res->cas)\n" \
"            ), array(\n" \
"                'persist_to' => $this->_persist,\n" \
"                'replicate_to' => $this->_replicate\n" \
"            ));\n" \
"\n" \
"            if ($dres) {\n" \
"                return $res;\n" \
"            } else {\n" \
"                throw new Exception('durability requirements failed');\n" \
"            }\n" \
"        }\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::insert() CouchbaseBucket::insert()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param $doc\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function insert($id, $doc, $options = array()) {\n" \
"        return $this->_endure($id,\n" \
"            $this->_bucket->insert($id, $doc, $options)\n" \
"        );\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::upsert() CouchbaseBucket::upsert()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param $doc\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function upsert($id, $doc, $options = array()) {\n" \
"        return $this->_endure($id,\n" \
"            $this->_bucket->upsert($id, $doc, $options)\n" \
"        );\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::save() CouchbaseBucket::save()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param $doc\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function save($id, $doc, $options = array()) {\n" \
"        return $this->_endure($id,\n" \
"            $this->_bucket->save($id, $doc, $options)\n" \
"        );\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::update() CouchbaseBucket::update()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param $updates\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function update($id, $updates, $options = array()) {\n" \
"        return $this->_endure($id,\n" \
"            $this->_bucket->update($id, $updates, $options)\n" \
"        );\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::remove() CouchbaseBucket::remove()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function remove($id, $options = array()) {\n" \
"        return $this->_endure($id,\n" \
"            $this->_bucket->remove($id, $options)\n" \
"        );\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::get() CouchbaseBucket::get()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function get($id, $options = array()) {\n" \
"        return $this->_bucket->get($id, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::find() CouchbaseBucket::find()\n" \
"     *\n" \
"     * @param $ddocview\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function find($ddocview, $options = array()) {\n" \
"        $options['stale'] = 'false';\n" \
"        return $this->_bucket->find($ddocview, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::unlock() CouchbaseBucket::unlock()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param $cas\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function unlock($id, $cas, $options = array()) {\n" \
"        return $this->_bucket->unlock($id, $cas, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::counter() CouchbaseBucket::counter()\n" \
"     *\n" \
"     * @param $id\n" \
"     * @param $delta\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function counter($id, $delta, $options = array()) {\n" \
"        return $this->_endure($id,\n" \
"            $this->_bucket->counter($id, $delta, $options)\n" \
"        );\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::view() CouchbaseBucket::view()\n" \
"     *\n" \
"     * @param $ddoc\n" \
"     * @param $view\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function view($ddoc, $view, $options = array()) {\n" \
"        return $this->_bucket->view($ddoc, $view, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::info() CouchbaseBucket::info()\n" \
"     *\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function info() {\n" \
"        return $this->_bucket->info();\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::flush() CouchbaseBucket::flush()\n" \
"     *\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function flush() {\n" \
"        return $this->_bucket->flush();\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * See referenced CouchbaseBucket parent method.\n" \
"     *\n" \
"     * @see CouchbaseBucket::endure() CouchbaseBucket::endure()\n" \
"     *\n" \
"     * @param array $reqs\n" \
"     * @return CouchbaseBucketDProxy\n" \
"     */\n" \
"    public function endure($reqs = array()) {\n" \
"        if (isset($reqs['persist_to'])) {\n" \
"            $reqs['persist_to'] = max($reqs['persist_to'], $this->_persist);\n" \
"        } else {\n" \
"            $reqs['persist_to'] = $this->_persist;\n" \
"        }\n" \
"        if (isset($reqs['replicate_to'])) {\n" \
"            $reqs['replicate_to'] = max($reqs['replicate_to'], $this->_persist);\n" \
"        } else {\n" \
"            $reqs['replicate_to'] = $this->_replicate;\n" \
"        }\n" \
"        return $this->_me->endure($reqs);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Magic function to handle the retrieval of various properties.  This\n" \
"     * simply proxies the request to the underlying bucket object.\n" \
"     *\n" \
"     * @internal\n" \
"     */\n" \
"    public function __get($name) {\n" \
"        return $this->_me->__get($name);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Magic function to handle the setting of various properties.  This\n" \
"     * simply proxies the request to the underlying bucket object.\n" \
"     *\n" \
"     * @internal\n" \
"     */\n" \
"    public function __set($name, $value) {\n" \
"        return $this->_me->__set($name, $value);\n" \
"    }\n" \
"}\n" \
"\n" \
"/**\n" \
" * Represents a bucket connection.\n" \
" *\n" \
" * Note: This class must be constructed by calling the openBucket\n" \
" * method of the CouchbaseCluster class.\n" \
" *\n" \
" * @property integer $operationTimeout\n" \
" * @property integer $viewTimeout\n" \
" * @property integer $durabilityInterval\n" \
" * @property integer $durabilityTimeout\n" \
" * @property integer $httpTimeout\n" \
" * @property integer $configTimeout\n" \
" * @property integer $configDelay\n" \
" * @property integer $configNodeTimeout\n" \
" * @property integer $htconfigIdleTimeout\n" \
" *\n" \
" * @package Couchbase\n" \
" *\n" \
" * @see CouchbaseCluster::openBucket()\n" \
" */\n" \
"class CouchbaseBucket {\n" \
"    /**\n" \
"     * @var _CouchbaseBucket\n" \
"     * @ignore\n" \
"     *\n" \
"     * Pointer to our C binding backing class.\n" \
"     */\n" \
"    private $me;\n" \
"\n" \
"    /**\n" \
"     * @var string\n" \
"     * @ignore\n" \
"     *\n" \
"     * The name of the bucket this object represents.\n" \
"     */\n" \
"    private $name;\n" \
"\n" \
"    /**\n" \
"     * Constructs a bucket connection.\n" \
"     *\n" \
"     * @private\n" \
"     * @ignore\n" \
"     *\n" \
"     * @param $hosts A list of hosts for the cluster.\n" \
"     * @param $name The name of the bucket to connect to.\n" \
"     * @param $password The password to authenticate with.\n" \
"     *\n" \
"     * @private\n" \
"     */\n" \
"    public function __construct($hosts, $name, $password, $ccpath) {\n" \
"        $this->me = new _CouchbaseBucket($hosts, $name, $password, $ccpath);\n" \
"        $this->me->setTranscoder(\"couchbase_default_encoder\", \"couchbase_default_decoder\");\n" \
"        $this->name = $name;\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Inserts a document.  This operation will fail if\n" \
"     * the document already exists on the cluster.\n" \
"     *\n" \
"     * @param string|array $ids\n" \
"     * @param mixed $val\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function insert($ids, $val = NULL, $options = array()) {\n" \
"        return $this->me->insert($ids, $val, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Inserts or updates a document, depending on whether the\n" \
"     * document already exists on the cluster.\n" \
"     *\n" \
"     * @param string|array $ids\n" \
"     * @param mixed $val\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function upsert($ids, $val = NULL, $options = array()) {\n" \
"        return $this->me->upsert($ids, $val, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Saves a document.\n" \
"     *\n" \
"     * @param string|array $ids\n" \
"     * @param mixed $val\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function save($ids, $val = NULL, $options = array()) {\n" \
"        return $this->me->save($ids, $val, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Deletes a document.\n" \
"     *\n" \
"     * @param string|array $ids\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function remove($ids, $options = array()) {\n" \
"        return $this->me->remove($ids, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Retrieves a document.\n" \
"     *\n" \
"     * @param string|array $ids\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function get($ids, $options = array()) {\n" \
"        return $this->me->get($ids, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Increment or decrements a key (based on $delta).\n" \
"     *\n" \
"     * @param string|array $ids\n" \
"     * @param integer $delta\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function counter($ids, $delta, $options = array()) {\n" \
"        return $this->me->counter($ids, $delta, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Unlocks a key previous locked with a call to get().\n" \
"     * @param string|array $ids\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function unlock($ids, $options = array()) {\n" \
"        return $this->me->unlock($ids, $options);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Finds a document by view request.\n" \
"     *\n" \
"     * @param string $ddocview\n" \
"     * @param array $options\n" \
"     * @return array\n" \
"     */\n" \
"    public function find($ddocview, $options) {\n" \
"        $info = explode('/', $ddocview);\n" \
"        $res = $this->view($info[0], $info[1], $options);\n" \
"        $out = array();\n" \
"\n" \
"        return $out;\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Performs a raw view request.\n" \
"     *\n" \
"     * @param string $ddoc\n" \
"     * @param string $view\n" \
"     * @param array $options\n" \
"     * @return mixed\n" \
"     * @throws CouchbaseException\n" \
"     */\n" \
"    public function view($ddoc, $view, $options) {\n" \
"        $path = '/_design/' . $ddoc . '/_view/' . $view;\n" \
"        $args = array();\n" \
"        foreach ($options as $option => $value) {\n" \
"            array_push($args, $option . '=' . json_encode($value));\n" \
"        }\n" \
"        $path .= '?' . implode('&', $args);\n" \
"        $res = $this->me->http_request(1, 1, $path, NULL, 1);\n" \
"        $out = json_decode($res, true);\n" \
"        if ($out['error']) {\n" \
"            throw new CouchbaseException($out['error'] . ': ' . $out['reason']);\n" \
"        }\n" \
"        return $out;\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Flushes a bucket (clears all data).\n" \
"     *\n" \
"     * @return mixed\n" \
"     */\n" \
"    public function flush() {\n" \
"        return $this->me->flush();\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Retrieves bucket status information\n" \
"     *\n" \
"     * Returns an associative array of status information as seen\n" \
"     * by the cluster for this bucket.  The exact structure of the\n" \
"     * returned data can be seen in the Couchbase Manual by looking\n" \
"     * at the bucket /info endpoint.\n" \
"     *\n" \
"     * @return mixed The status information.\n" \
"     */\n" \
"    public function info() {\n" \
"        $path = \"/pools/default/buckets/\" . $this->name;\n" \
"        $res = $this->me->http_request(2, 1, $path, NULL, 2);\n" \
"        return json_decode($res, true);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Creates a proxy bucket object that will ensure a certain\n" \
"     * level of durability is achieved.\n" \
"     *\n" \
"     * @param array $reqs\n" \
"     * @return CouchbaseBucketDProxy\n" \
"     */\n" \
"    public function endure($reqs = array()) {\n" \
"        $persist = 0;\n" \
"        if (isset($reqs['persist_to'])) {\n" \
"            $persist = $reqs['persist_to'];\n" \
"        }\n" \
"        $replicate = 0;\n" \
"        if (isset($reqs['replicate_to'])) {\n" \
"            $replicate = $reqs['replicate_to'];\n" \
"        }\n" \
"        return new CouchbaseBucketDProxy($this->me, $this, $persist, $replicate);\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Magic function to handle the retrieval of various properties.\n" \
"     *\n" \
"     * @internal\n" \
"     */\n" \
"    public function __get($name) {\n" \
"        if ($name == 'operationTimeout') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_OP_TIMEOUT);\n" \
"        } else if ($name == 'viewTimeout') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_VIEW_TIMEOUT);\n" \
"        } else if ($name == 'durabilityInterval') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_DURABILITY_INTERVAL);\n" \
"        } else if ($name == 'durabilityTimeout') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_DURABILITY_TIMEOUT);\n" \
"        } else if ($name == 'httpTimeout') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_HTTP_TIMEOUT);\n" \
"        } else if ($name == 'configTimeout') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_CONFIGURATION_TIMEOUT);\n" \
"        } else if ($name == 'configDelay') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_CONFDELAY_THRESH);\n" \
"        } else if ($name == 'configNodeTimeout') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_CONFIG_NODE_TIMEOUT);\n" \
"        } else if ($name == 'htconfigIdleTimeout') {\n" \
"            return $this->me->getOption(COUCHBASE_CNTL_HTCONFIG_IDLE_TIMEOUT);\n" \
"        }\n" \
"\n" \
"\n" \
"        $trace = debug_backtrace();\n" \
"        trigger_error(\n" \
"            'Undefined property via __get(): ' . $name .\n" \
"            ' in ' . $trace[0]['file'] .\n" \
"            ' on line ' . $trace[0]['line'],\n" \
"            E_USER_NOTICE);\n" \
"        return null;\n" \
"    }\n" \
"\n" \
"    /**\n" \
"     * Magic function to handle the setting of various properties.\n" \
"     *\n" \
"     * @internal\n" \
"     */\n" \
"    public function __set($name, $value) {\n" \
"        if ($name == 'operationTimeout') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_OP_TIMEOUT, $value);\n" \
"        } else if ($name == 'viewTimeout') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_VIEW_TIMEOUT, $value);\n" \
"        } else if ($name == 'durabilityInterval') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_DURABILITY_INTERVAL, $value);\n" \
"        } else if ($name == 'durabilityTimeout') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_DURABILITY_TIMEOUT, $value);\n" \
"        } else if ($name == 'httpTimeout') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_HTTP_TIMEOUT, $value);\n" \
"        } else if ($name == 'configTimeout') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_CONFIGURATION_TIMEOUT, $value);\n" \
"        } else if ($name == 'configDelay') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_CONFDELAY_THRESH, $value);\n" \
"        } else if ($name == 'configNodeTimeout') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_CONFIG_NODE_TIMEOUT, $value);\n" \
"        } else if ($name == 'htconfigIdleTimeout') {\n" \
"            return $this->me->setOption(COUCHBASE_CNTL_HTCONFIG_IDLE_TIMEOUT, $value);\n" \
"        }\n" \
"\n" \
"\n" \
"        $trace = debug_backtrace();\n" \
"        trigger_error(\n" \
"            'Undefined property via __set(): ' . $name .\n" \
"            ' in ' . $trace[0]['file'] .\n" \
"            ' on line ' . $trace[0]['line'],\n" \
"            E_USER_NOTICE);\n" \
"        return null;\n" \
"    }\n" \
"}\n" \
"\n" \
"/*\n" \
" * The following is a list of constants used for flags and datatype\n" \
" * encoding and decoding by the built in transcoders.\n" \
" */\n" \
"\n" \
"/** @internal */ define('COUCHBASE_VAL_MASK', 0x1F);\n" \
"/** @internal */ define('COUCHBASE_VAL_IS_STRING', 0);\n" \
"/** @internal */ define('COUCHBASE_VAL_IS_LONG', 1);\n" \
"/** @internal */ define('COUCHBASE_VAL_IS_DOUBLE', 2);\n" \
"/** @internal */ define('COUCHBASE_VAL_IS_BOOL', 3);\n" \
"/** @internal */ define('COUCHBASE_VAL_IS_SERIALIZED', 4);\n" \
"/** @internal */ define('COUCHBASE_VAL_IS_IGBINARY', 5);\n" \
"/** @internal */ define('COUCHBASE_VAL_IS_JSON', 6);\n" \
"/** @internal */ define('COUCHBASE_COMPRESSION_MASK', 0x7 << 5);\n" \
"/** @internal */ define('COUCHBASE_COMPRESSION_NONE', 0 << 5);\n" \
"/** @internal */ define('COUCHBASE_COMPRESSION_ZLIB', 1 << 5);\n" \
"/** @internal */ define('COUCHBASE_COMPRESSION_FASTLZ', 2 << 5);\n" \
"/** @internal */ define('COUCHBASE_COMPRESSION_MCISCOMPRESSED', 1 << 4);\n" \
"/** @internal */ define('COUCHBASE_SERTYPE_JSON', 0);\n" \
"/** @internal */ define('COUCHBASE_SERTYPE_IGBINARY', 1);\n" \
"/** @internal */ define('COUCHBASE_SERTYPE_PHP', 2);\n" \
"/** @internal */ define('COUCHBASE_CMPRTYPE_NONE', 0);\n" \
"/** @internal */ define('COUCHBASE_CMPRTYPE_ZLIB', 1);\n" \
"/** @internal */ define('COUCHBASE_CMPRTYPE_FASTLZ', 2);\n" \
"\n" \
"/**\n" \
" * The default options for V1 encoding when using the default\n" \
" * transcoding functionality.\n" \
" * @internal\n" \
" */\n" \
"$COUCHBASE_DEFAULT_ENCOPTS = array(\n" \
"    'sertype' => COUCHBASE_SERTYPE_PHP,\n" \
"    'cmprtype' => COUCHBASE_CMPRTYPE_NONE,\n" \
"    'cmprthresh' => 2000,\n" \
"    'cmprfactor' => 1.3\n" \
");\n" \
"\n" \
"/**\n" \
" * Performs encoding of user provided types into binary form for\n" \
" * on the server according to the original PHP SDK specification.\n" \
" *\n" \
" * @internal\n" \
" *\n" \
" * @param $value The value passed by the user\n" \
" * @param $options Various encoding options\n" \
" * @return array An array specifying the bytes, flags and datatype to store\n" \
" */\n" \
"function couchbase_basic_encoder_v1($value, $options) {\n" \
"    $data = NULL;\n" \
"    $flags = 0;\n" \
"    $datatype = 0;\n" \
"\n" \
"    $sertype = $options['sertype'];\n" \
"    $cmprtype = $options['cmprtype'];\n" \
"    $cmprthresh = $options['cmprthresh'];\n" \
"    $cmprfactor = $options['cmprfactor'];\n" \
"\n" \
"    $vtype = gettype($value);\n" \
"    if ($vtype == 'string') {\n" \
"        $flags = COUCHBASE_VAL_IS_STRING;\n" \
"        $data = $value;\n" \
"    } else if ($vtype == 'integer') {\n" \
"        $flags = COUCHBASE_VAL_IS_LONG;\n" \
"        $data = (string)$value;\n" \
"        $cmprtype = COUCHBASE_CMPRTYPE_NONE;\n" \
"    } else if ($vtype == 'double') {\n" \
"        $flags = COUCHBASE_VAL_IS_DOUBLE;\n" \
"        $data = (string)$value;\n" \
"        $cmprtype = COUCHBASE_CMPRTYPE_NONE;\n" \
"    } else if ($vtype == 'boolean') {\n" \
"        $flags = COUCHBASE_VAL_IS_BOOL;\n" \
"        $data = (string)$value;\n" \
"        $cmprtype = COUCHBASE_CMPRTYPE_NONE;\n" \
"    } else {\n" \
"        if ($sertype == COUCHBASE_SERTYPE_JSON) {\n" \
"            $flags = COUCHBASE_VAL_IS_JSON;\n" \
"            $data = json_encode($value);\n" \
"        } else if ($sertype == COUCHBASE_SERTYPE_IGBINARY) {\n" \
"            $flags = COUCHBASE_VAL_IS_IGBINARY;\n" \
"            $data = igbinary_serialize($value);\n" \
"        } else if ($sertype == COUCHBASE_SERTYPE_PHP) {\n" \
"            $flags = COUCHBASE_VAL_IS_SERIALIZED;\n" \
"            $data = serialize($value);\n" \
"        }\n" \
"    }\n" \
"\n" \
"    if (strlen($data) < $cmprthresh) {\n" \
"        $cmprtype = COUCHBASE_CMPRTYPE_NONE;\n" \
"    }\n" \
"\n" \
"    if ($cmprtype != COUCHBASE_CMPRTYPE_NONE) {\n" \
"        $cmprdata = NULL;\n" \
"        $cmprflags = COUCHBASE_COMPRESSION_NONE;\n" \
"\n" \
"        if ($cmprtype == COUCHBASE_CMPRTYPE_ZLIB) {\n" \
"            $cmprdata = gzencode($data);\n" \
"            $cmprflags = COUCHBASE_COMPRESSION_ZLIB;\n" \
"        } else if ($cmprtype == COUCHBASE_CMPRTYPE_FASTLZ) {\n" \
"            $cmprdata = fastlz_compress($data);\n" \
"            $cmprflags = COUCHBASE_COMPRESSION_FASTLZ;\n" \
"        }\n" \
"\n" \
"        if ($cmprdata != NULL) {\n" \
"            if (strlen($data) > strlen($cmprdata) * $cmprfactor) {\n" \
"                $data = $cmprdata;\n" \
"                $flags |= $cmprflags;\n" \
"                $flags |= COUCHBASE_COMPRESSION_MCISCOMPRESSED;\n" \
"            }\n" \
"        }\n" \
"    }\n" \
"\n" \
"    return array($data, $flags, $datatype);\n" \
"}\n" \
"\n" \
"/**\n" \
" * Performs decoding of the server provided binary data into\n" \
" * PHP types according to the original PHP SDK specification.\n" \
" *\n" \
" * @internal\n" \
" *\n" \
" * @param $bytes The binary received from the server\n" \
" * @param $flags The flags received from the server\n" \
" * @param $datatype The datatype received from the server\n" \
" * @return mixed The resulting decoded value\n" \
" */\n" \
"function couchbase_basic_decoder_v1($bytes, $flags, $datatype) {\n" \
"    $sertype = $flags & COUCHBASE_VAL_MASK;\n" \
"    $cmprtype = $flags & COUCHBASE_COMPRESSION_MASK;\n" \
"\n" \
"    $data = $bytes;\n" \
"    if ($cmprtype == COUCHBASE_COMPRESSION_ZLIB) {\n" \
"        $bytes = gzdecode($bytes);\n" \
"    } else if ($cmprtype == COUCHBASE_COMPRESSION_FASTLZ) {\n" \
"        $data = fastlz_decompress($bytes);\n" \
"    }\n" \
"\n" \
"    $retval = NULL;\n" \
"    if ($sertype == COUCHBASE_VAL_IS_STRING) {\n" \
"        $retval = $data;\n" \
"    } else if ($sertype == COUCHBASE_VAL_IS_LONG) {\n" \
"        $retval = intval($data);\n" \
"    } else if ($sertype == COUCHBASE_VAL_IS_DOUBLE) {\n" \
"        $retval = floatval($data);\n" \
"    } else if ($sertype == COUCHBASE_VAL_IS_BOOL) {\n" \
"        $retval = boolval($data);\n" \
"    } else if ($sertype == COUCHBASE_VAL_IS_JSON) {\n" \
"        $retval = json_decode($data);\n" \
"    } else if ($sertype == COUCHBASE_VAL_IS_IGBINARY) {\n" \
"        $retval = igbinary_unserialize($data);\n" \
"    } else if ($sertype == COUCHBASE_VAL_IS_SERIALIZED) {\n" \
"        $retval = unserialize($data);\n" \
"    }\n" \
"\n" \
"    return $retval;\n" \
"}\n" \
"\n" \
"/**\n" \
" * Default passthru encoder which simply passes data\n" \
" * as-is rather than performing any transcoding.\n" \
" *\n" \
" * @internal\n" \
" */\n" \
"function couchbase_passthru_encoder($value) {\n" \
"    return $value;\n" \
"}\n" \
"\n" \
"/**\n" \
" * Default passthru encoder which simply passes data\n" \
" * as-is rather than performing any transcoding.\n" \
" *\n" \
" * @internal\n" \
" */\n" \
"function couchbase_passthru_decoder($bytes, $flags, $datatype) {\n" \
"    return $bytes;\n" \
"}\n" \
"\n" \
"/**\n" \
" * The default encoder for the client.  Currently invokes the\n" \
" * v1 encoder directly with the default set of encoding options.\n" \
" *\n" \
" * @internal\n" \
" */\n" \
"function couchbase_default_encoder($value) {\n" \
"    global $COUCHBASE_DEFAULT_ENCOPTS;\n" \
"    return couchbase_basic_encoder_v1($value, $COUCHBASE_DEFAULT_ENCOPTS);\n" \
"}\n" \
"\n" \
"/**\n" \
" * The default decoder for the client.  Currently invokes the\n" \
" * v1 decoder directly.\n" \
" *\n" \
" * @internal\n" \
" */\n" \
"function couchbase_default_decoder($bytes, $flags, $datatype) {\n" \
"    return couchbase_basic_decoder_v1($bytes, $flags, $datatype);\n" \
"}\n" \
"";
