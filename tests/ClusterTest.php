<?php
require_once('CouchbaseTestCase.php');

class ClusterTest extends CouchbaseTestCase {
    /**
     * @test
     * Test to stop phpunit from complaining about no tests
     */
    function testNothing() {
    }
 
    /**
     * @test
     * Test that connections with invalid details fail.
     *
     * @expectedException CouchbaseException
     */
    function testBadHost() {
        $h = new CouchbaseCluster('http://999.99.99.99:8091');
        $h->openBucket('default');
    }

    /**
     * //@test
     * Test that connections with invalid details fail.
     *
     * @expectedException CouchbaseException
     * @expectedExceptionMessage Unknown host
     */
    //function testBadUser() {
    //    $h = new CouchbaseCluster('localhost:8091', 'bad_user', 'bad_pass');
    //    $h->connect();
    //}

    /**
     * //@test
     * Test that connections with invalid details fail.
     *
     * @expectedException CouchbaseException
     * @expectedExceptionMessage Unknown host
     */
    //function testBadPass() {
    //    $h = new CouchbaseCluster('localhost:8091', 'Administrator', 'bad_pass');
    //    $h->connect();
    //}

    /**
     * @test
     * Test that a query will execute successfully
     */
    //function testQuery() {
    //    $h = new CouchbaseCluster('localhost:8091');
    //    $q = $h->openQuerier('localhost:8093');
    //    
    //    $resset = $q->query('SELECT * FROM default');
    //    
    //    $this->assertNotEmpty($resset);
    //}
    
    /**
     * @test
     * Test that a query will fail successfully
     */
    //function testBadQuery() {
    //    $h = new CouchbaseCluster('localhost:8091');
    //    $q = $h->openQuerier('localhost:8093');
    //    
    //    
    //    $this->wrapException(function() use($h, $q) {
    //        $resset = $q->query('SELECT * FROM invalid_bucket');
    //    }, 'CouchbaseException', 999);
    //}

}
